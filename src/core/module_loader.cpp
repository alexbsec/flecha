#include "core/module_loader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "core/parser.hpp"
#include "core/tokenizer.hpp"

namespace flecha::core {
namespace fs = std::filesystem;
void ModuleLoader::Report(source::SourceRange range,
                          const std::string& message) {
    _diagnostics.Report(diagnostics::DiagnosticCode::UnknownName, range,
                        message);
}
std::unique_ptr<CompilationUnit> ModuleLoader::Load(
    const fs::path& entry, fs::path root,
    std::map<std::string, fs::path> libraries) {
    _loaded.clear();
    _roots.clear();
    _active.clear();
    _modules.clear();
    _declarations.clear();
    _libraries = std::move(libraries);
    if (root.empty()) {
        root = fs::absolute(entry).parent_path();
        for (fs::path directory = root; !directory.empty();
             directory = directory.parent_path()) {
            if (directory.filename() == "src") {
                root = directory;
                break;
            }
            if (directory == directory.parent_path()) break;
        }
    }
    Visit(entry, fs::weakly_canonical(root), true);
    auto unit = std::make_unique<CompilationUnit>(source::SourceRange{},
                                                  std::move(_declarations));
    unit->modules = std::move(_modules);
    return unit;
}
std::string ModuleLoader::Visit(const fs::path& input, const fs::path& root,
                                bool entry, source::SourceRange importRange) {
    const fs::path file = fs::weakly_canonical(input);
    if (std::find(_active.begin(), _active.end(), file) != _active.end()) {
        std::string cycle;
        for (const auto& path : _active) cycle += path.string() + " -> ";
        Report(importRange, "module import cycle: " + cycle + file.string());
        return {};
    }
    if (const auto found = _loaded.find(file); found != _loaded.end()) {
        if (_roots.at(file) != root)
            Report(importRange,
                   "module is imported with conflicting source roots: " +
                       file.string());
        return found->second;
    }
    if (_active.size() >= 256) {
        Report(importRange, "module import depth exceeds 256");
        return {};
    }
    std::ifstream stream(file);
    if (!stream) {
        Report(importRange, "cannot open module '" + file.string() + "'");
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string source = buffer.str();
    diagnostics::DiagnosticBag lexical;
    auto tokens = Tokenizer(source, lexical).Tokenize();
    const auto locate = [&](source::SourceRange range) {
        range.start.file = file.string();
        range.end.file = file.string();
        return range;
    };
    for (auto& token : tokens) token.range = locate(token.range);
    for (const auto& diagnostic : lexical.Diagnostics())
        _diagnostics.Report(diagnostic.code, locate(diagnostic.range),
                            diagnostic.message, diagnostic.severity);
    Parser parser(std::move(tokens), _diagnostics);
    parser.ParseHeader();
    std::vector<Import> expanded;
    for (const auto& import : parser.Imports()) {
        if (!import.wildcard) {
            expanded.push_back(import);
            continue;
        }
        fs::path sourceRoot = root;
        std::size_t start = 1;
        if (import.path[0] == "lib") {
            if (import.path.size() < 2) continue;
            const auto library = _libraries.find(import.path[1]);
            if (library == _libraries.end()) {
                Report(import.range,
                       "library '" + import.path[1] + "' is not configured");
                continue;
            }
            sourceRoot = fs::weakly_canonical(library->second);
            start = 2;
        } else if (import.path[0] != "root")
            continue;
        fs::path directory = sourceRoot;
        for (std::size_t i = start; i < import.path.size(); ++i)
            directory /= import.path[i];
        directory = fs::weakly_canonical(directory);
        const auto relative = directory.lexically_relative(sourceRoot);
        if (relative.empty() || *relative.begin() == "..") {
            Report(import.range, "module path escapes its source root");
            continue;
        }
        if (!fs::is_directory(directory)) {
            Report(import.range,
                   "cannot open module directory '" + directory.string() + "'");
            continue;
        }
        std::vector<std::string> names;
        for (const auto& item : fs::directory_iterator(directory))
            if (item.is_regular_file() && item.path().extension() == ".fl")
                names.push_back(item.path().stem().string());
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            diagnostics::DiagnosticBag nameDiagnostics;
            const auto nameTokens = Tokenizer(name, nameDiagnostics).Tokenize();
            if (nameDiagnostics.HasErrors() || nameTokens.size() != 2 ||
                nameTokens[0].type != TokenType::Identifier ||
                nameTokens[0].value != name || name == "root" ||
                name == "lib") {
                Report(import.range,
                       "invalid module filename '" + name + ".fl'");
                continue;
            }
            Import selected = import;
            selected.wildcard = false;
            selected.path.push_back(name);
            selected.alias = name;
            expanded.push_back(std::move(selected));
        }
    }
    parser.SetImports(std::move(expanded));
    auto unit = parser.Parse();
    if (unit->moduleName.empty() && !entry)
        Report(unit->Range(), "imported files require a module declaration");
    if (!unit->moduleName.empty() && unit->moduleName != file.stem().string())
        Report(unit->Range(), "module '" + unit->moduleName +
                                  "' must match filename '" +
                                  file.stem().string() + "'");
    // Legacy standalone files retain their native names and ABI. Declared
    // modules get an inaccessible internal prefix, including private functions.
    const std::string id = unit->moduleName.empty() && entry
                               ? ""
                               : "$m" + std::to_string(_loaded.size()) + "$";
    _loaded.emplace(file, id);
    _roots.emplace(file, root);
    _active.push_back(file);
    Module module{id, unit->moduleName, file.string(), entry,
                  std::move(unit->imports)};
    for (auto& import : module.imports) {
        if (import.path.empty()) continue;
        fs::path sourceRoot = root;
        std::size_t start = 1;
        if (import.path[0] == "lib") {
            if (import.path.size() < 3) continue;
            const auto library = _libraries.find(import.path[1]);
            if (library == _libraries.end()) {
                Report(
                    import.range,
                    "library '" + import.path[1] +
                        "' is not configured; supply --lib name=source-root");
                continue;
            }
            sourceRoot = fs::weakly_canonical(library->second);
            start = 2;
        } else if (import.path[0] != "root" || import.path.size() < 2)
            continue;
        fs::path target = sourceRoot;
        for (std::size_t index = start; index < import.path.size(); ++index)
            target /= import.path[index];
        target += ".fl";
        const auto relative =
            fs::weakly_canonical(target).lexically_relative(sourceRoot);
        if (relative.empty() || *relative.begin() == "..") {
            Report(import.range, "module path escapes its source root");
            continue;
        }
        import.resolvedModule = Visit(target, sourceRoot, false, import.range);
    }
    _active.pop_back();
    for (auto& declaration : unit->TakeDeclarations()) {
        declaration->moduleId = id;
        _declarations.push_back(std::move(declaration));
    }
    _modules.push_back(std::move(module));
    return id;
}
}  // namespace flecha::core
