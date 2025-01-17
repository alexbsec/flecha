#ifndef FLECHA_AST_HPP
#define FLECHA_AST_HPP

#include <string>
#include <vector>

// Aliases
template <typename... Args>
using vector = std::vector<Args...>;
using string = std::string;

namespace flecha {
namespace core {

/**
 * @brief Visitor class to help traverse AST
 */
class Visitor {
   public:
    // Variables
    virtual void Visit(class VariableNode& node) = 0;
    virtual void Visit(class ValueNode& node) = 0;

    // Location
    virtual void Visit(class StartNode& node) = 0;
    virtual void Visit(class EndNode& node) = 0;
    virtual void Visit(class LocationNode& node) = 0;
    virtual void Visit(class RangeNode& node) = 0;

    // Program
    virtual void Visit(class ProgramNode& node) = 0;
    virtual void Visit(class ProgramInitializationNode& node) = 0;
    virtual void Visit(class BodyNode& node) = 0;

    // Expressions
    virtual void Visit(class AllocationStatementNode& node) = 0;
    virtual void Visit(class VariableDeclarationNode& node) = 0;

    // Initializations
    virtual void Visit(class InitializationStatementNode& node) = 0;

    // Memory
    virtual void Visit(class PointerNode& node) = 0;
    virtual void Visit(class MemoryNode& node) = 0;
    virtual void Visit(class AllocationNode& node) = 0;

    // Types
    virtual void Visit(class PrimitiveTypeNode& node) = 0;
    virtual void Visit(class UserDefinedTypeNode& node) = 0;

    /**
     * @brief Default destructor
     */
    virtual ~Visitor() = default;
};

/**
 * @brief Main AST node
 */
struct ASTNode {
    /**
     * @brief Default destructor
     */
    virtual ~ASTNode() = default;
    /**
     * @brief Default accept method
     */
    virtual void Accept(class Visitor& visitor) = 0;
};

/* Location Nodes */

struct StartNode : ASTNode {
    int line;
    int column;

    /**
     * @brief The StartNode constructor
     *
     * @param l - The line
     * @param c - The column
     */
    StartNode(int l, int c) : line(l), column(c) {}

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct EndNode : ASTNode {
    int line;
    int column;

    /**
     * @brief The EndNode constructor
     *
     * @param l - The line
     * @param c - The column
     */
    EndNode(int l, int c) : line(l), column(c) {}

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct LocationNode : ASTNode {
    ASTNode* start;
    ASTNode* end;

    /**
     * @brief The LocationNode constructor
     *
     * @param st - The start node
     * @param end - The end node
     */
    LocationNode(ASTNode* st, ASTNode* end) : start(st), end(end) {}

    /**
     * @brief Destructs and frees memory
     */
    ~LocationNode() {
        if (start) {
            delete start;
            start = nullptr;
        }

        if (end) {
            delete end;
            end = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct RangeNode : ASTNode {
    std::pair<unsigned int, unsigned int> range;

    /**
     * @brief The RangeNode constructor
     *
     * @param start - the start of file
     * @param end - the end of file
     */
    RangeNode(int start, int end) { range = std::make_pair(start, end); }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* Body Node */

struct BodyNode : ASTNode {
    ASTNode* program_init;
    vector<ASTNode*> expressions;

    /**
     * @brief The BodyNode constructor
     *
     * @param p_init - The ProgramInitializationNode
     * @param exps - A vector contaning all ExpressionNodes
     */
    BodyNode(ASTNode* p_init, const vector<ASTNode*>& exps)
        : program_init(p_init), expressions(exps) {}

    ~BodyNode() {
        if (program_init) {
            delete program_init;
            program_init = nullptr;
        }

        for (auto& node : expressions) {
            if (node) {
                delete node;
                node = nullptr;
            }
        }

        expressions.clear();
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* Program Node */

struct ProgramInitializationNode : ASTNode {
    string package_name;

    ProgramInitializationNode(const string& name) : package_name(name) {}

    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct ProgramNode : ASTNode {
    ASTNode* body;
    ASTNode* location;
    ASTNode* range;

    /**
     * @brief The ProgramNode constructor
     *
     * @param body - The body node
     * @param loc - The location node
     * @param range - The range node
     */
    ProgramNode(ASTNode* body, ASTNode* loc, ASTNode* range)
        : body(body), location(loc), range(range) {}

    /**
     * @brief Destructs ProgramNode and free memory
     */
    ~ProgramNode() {
        if (body) {
            delete body;
            body = nullptr;
        }

        if (location) {
            delete location;
            location = nullptr;
        }

        if (range) {
            delete range;
            range = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* Expression Nodes */

/**
 * @brief Expression Node Interface
 */
struct ExpressionNode : ASTNode {
    virtual ~ExpressionNode() = default;
};

/**
 * @brief The allocation mode
 *
 * @example: int! var = allot(int);
 */
struct AllocationStatementNode : ExpressionNode {
    ASTNode* location;
    ASTNode* allocation;
    ASTNode* initialization;

    /**
     * @brief The AllocationStatementNode constructor
     *
     * @param loc - The location node
     * @param alloc - The allocation node
     * @param init -  The initialization node
     */
    AllocationStatementNode(ASTNode* loc, ASTNode* alloc, ASTNode* init)
        : location(loc), allocation(alloc), initialization(init) {}

    /**
     * @brief Destructs and free memory
     */
    ~AllocationStatementNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (allocation) {
            delete allocation;
            allocation = nullptr;
        }

        if (initialization) {
            delete initialization;
            initialization = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) { visitor.Visit(*this); }
};

struct VariableDeclarationNode : ExpressionNode {
    ASTNode* location;
    ASTNode* assignment;

    /**
     * @brief The VariableDeclarationNode constructor
     *
     * @param loc - The location node
     * @param assg - The assignment node
     */
    VariableDeclarationNode(ASTNode* loc, ASTNode* assg)
        : location(loc), assignment(assg) {}

    /**
     * @brief Destructs and free memory
     */
    ~VariableDeclarationNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (assignment) {
            delete assignment;
            assignment = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* General Nodes */

struct InitializationNode : ASTNode {
    virtual ~InitializationNode() = default;
};

struct InitializationStatementNode : InitializationNode {
    ASTNode* location;
    ASTNode* pointer_node;

    /**
     * @brief The InitializationStatementNode constructor
     *
     * @param loc - The location node
     * @param ptr - The pointer node
     */
    InitializationStatementNode(ASTNode* loc, ASTNode* ptr)
        : location(loc), pointer_node(ptr) {}

    /**
     * @brief Destructs and free memory
     */
    ~InitializationStatementNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (pointer_node) {
            delete pointer_node;
            pointer_node = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* Type nodes */

struct TypeNode : ASTNode {
    virtual ~TypeNode() = default;
    virtual string GetTypeName() const = 0;
    virtual bool IsPrimitive() const = 0;
};

struct PrimitiveTypeNode : TypeNode {
    string name;

    /**
     * @brief The PrimitiveTypeNode constructor
     *
     * @param name - The type name
     */
    PrimitiveTypeNode(const string& name) : name(name) {}

    /**
     * @brief Gets the type name of primitive type
     *
     * @return the type name
     */
    string GetTypeName() const override { return name; }

    /**
     * @brief Check if type is primitive
     *
     * @return True for primitive types
     */
    bool IsPrimitive() const override { return true; }

    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct UserDefinedTypeNode : TypeNode {
    string name;

    /**
     * @brief The UserDefinedTypeNode constructor
     *
     * @param name - The type name
     */
    UserDefinedTypeNode(const string& name) : name(name) {}

    /**
     * @brief Gets the type name of user defined type
     *
     * @return The type name
     */
    string GetTypeName() const override { return name; }

    /**
     * @brief Check if type is primitive
     *
     * @return False for user defined types
     */
    bool IsPrimitive() const override { return false; }

    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

/* Memory nodes */

struct AllocationNode : ASTNode {
    ASTNode* location;
    ASTNode* pointer_node;

    /**
     * @brief The AllocationNode constructor
     *
     * @param loc - The location node
     * @param ptr - The pointer node
     */
    AllocationNode(ASTNode* loc, ASTNode* ptr)
        : location(loc), pointer_node(ptr) {}

    /**
     * @brief Destructs and free memory
     */
    ~AllocationNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (pointer_node) {
            delete pointer_node;
            pointer_node = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct PointerNode : ASTNode {
    ASTNode* location;
    ASTNode* type;
    ASTNode* memory;
    ASTNode* variable;

    /**
     * @brief The PointerNode constructor
     *
     * @param loc - The location node
     * @param type - The type node
     * @param mem - The memory node
     */
    PointerNode(ASTNode* loc, ASTNode* type, ASTNode* mem, ASTNode* var)
        : location(loc), type(type), memory(mem), variable(var) {}

    /**
     * @brief Destructs and free memory
     */
    ~PointerNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        // We don't delete the type node because
        // its shared with the variable->value node

        if (memory) {
            delete memory;
            memory = nullptr;
        }

        if (variable) {
            delete variable;
            variable = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct MemoryNode : ASTNode {
    ASTNode* location;
    void* address;

    /**
     * @brief Allocates memory for a given size and stores the address
     *
     * @param loc - The AST location node
     * @param size - The size of memory to allocate
     */
    MemoryNode(ASTNode* loc, size_t size) : location(loc), address(nullptr) {
        address = malloc(size);  // Allocate memory
        if (!address) {
            throw std::bad_alloc();  // Handle allocation failure
        }
    }

    /**
     * @brief Frees the allocated memory
     */
    ~MemoryNode() {
        free(address);

        if (location) {
            delete location;
            location = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct VariableNode : ASTNode {
    string name;
    ASTNode* location;
    ASTNode* value;

    /**
     * @brief The VariableNode constructor
     *
     * @param name - The variable name
     * @param loc - The location node
     * @param val - The value node
     */
    VariableNode(const string& name, ASTNode* loc, ASTNode* val)
        : name(name), location(loc), value(val) {}

    /**
     * @brief Destructs and free memory
     */
    ~VariableNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (value) {
            delete value;
            value = nullptr;
        }
    }

    /**
     * @brief The Accept visitor for traversal
     *
     * @param visitor - The visitor class object
     */
    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};

struct ValueNode : ASTNode {
    string value;
    ASTNode* location;
    ASTNode* type;

    /**
     * @brief The ValueNode constructor
     *
     * @param val - The value as string
     * @param loc - The location node
     * @param type - The type node
     */
    ValueNode(const string& val, ASTNode* loc, ASTNode* type)
        : value(val), location(loc), type(type) {}

    /**
     * @brief Destructs and free memory
     */
    ~ValueNode() {
        if (location) {
            delete location;
            location = nullptr;
        }

        if (type) {
            delete type;
            type = nullptr;
        }
    }

    void Accept(Visitor& visitor) override { visitor.Visit(*this); }
};
}  // namespace core
}  // namespace flecha
#endif  // FLECHA_AST_HPP
