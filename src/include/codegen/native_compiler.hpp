#ifndef FLECHA_CODEGEN_NATIVE_COMPILER_HPP
#define FLECHA_CODEGEN_NATIVE_COMPILER_HPP

#include <memory>
#include <string>

#include <llvm/Target/TargetMachine.h>

#include "lowering/bound_ir.hpp"

namespace flecha::codegen {

class NativeCompiler final {
   public:
    NativeCompiler();

    [[nodiscard]] bool Ready() const { return _targetMachine != nullptr; }
    [[nodiscard]] const std::string& TargetTriple() const {
        return _targetTriple;
    }
    [[nodiscard]] const std::string& Error() const { return _error; }

    bool EmitObject(const lowering::BoundProgram& program,
                    const std::string& moduleName,
                    const std::string& objectPath);
    bool LinkExecutable(const std::string& objectPath,
                        const std::string& outputPath);
    bool BuildExecutable(const lowering::BoundProgram& program,
                         const std::string& moduleName,
                         const std::string& outputPath);

   private:
    bool Fail(std::string message);

    std::string _targetTriple;
    std::unique_ptr<llvm::TargetMachine> _targetMachine;
    std::string _error;
};

}  // namespace flecha::codegen

#endif  // FLECHA_CODEGEN_NATIVE_COMPILER_HPP
