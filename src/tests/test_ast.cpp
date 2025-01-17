#include <gtest/gtest.h>

#include <sstream>
#include <vector>

#include "core/AST.hpp"

using namespace flecha::core;

/* LOCATION NODE */

TEST(ASTLocationTests, TestLocationNode) {
    /* :main
     *
     * int! var = allot(int);
     */
    StartNode* start = new StartNode(1, 5);
    EndNode* end = new EndNode(3, 22);
    LocationNode* location = new LocationNode(start, end);

    // Validate start node
    ASSERT_EQ(start->line, 1);
    ASSERT_EQ(start->column, 5);

    ASSERT_EQ(end->line, 3);
    ASSERT_EQ(end->column, 22);

    delete location;
}

TEST(ASTLocationTests, LocationDestructorReleaseMemory) {
    StartNode* start = new StartNode(1, 2);
    EndNode* end = new EndNode(2, 4);

    LocationNode* location = new LocationNode(start, end);

    ASSERT_NO_THROW(delete location);
}

/* RANGE NODE */

TEST(ASTRangeTests, RangeConstructorAndAccessor) {
    RangeNode* range = new RangeNode(0, 33);

    ASSERT_EQ(range->range.first, 0);
    ASSERT_EQ(range->range.second, 33);

    delete range;
}

/* TYPE NODE */

TEST(ASTPrimitiveType, PrimitiveTypeConstructorAndFunctions) {
    PrimitiveTypeNode* type = new PrimitiveTypeNode("int!");

    string got_name = type->GetTypeName();
    bool got_type = type->IsPrimitive();

    ASSERT_EQ(got_name, "int!");
    ASSERT_EQ(got_type, true);

    delete type;
}

TEST(ASTUserDefinedType, UserDefinedTypeConstructorAndFunctions) {
    UserDefinedTypeNode* type = new UserDefinedTypeNode("MyType!");

    string got_name = type->GetTypeName();
    bool got_type = type->IsPrimitive();

    ASSERT_EQ(got_name, "MyType!");
    ASSERT_EQ(got_type, false);

    delete type;
}

/* MEMORY NODE */

TEST(ASTMemoryNode, MemoryAllocateTest) {
    StartNode* start = new StartNode(0, 1);
    EndNode* end = new EndNode(2, 5);
    LocationNode* location = new LocationNode(start, end);
    size_t alloc_size = sizeof(int);

    MemoryNode* mem = new MemoryNode(location, alloc_size);

    ASSERT_NE(mem->address, nullptr);

    int* int_addr = static_cast<int*>(mem->address);
    *int_addr = 42;
    ASSERT_EQ(*int_addr, 42);

    delete mem;
}

TEST(ASTMemoryNode, MemoryDestructorTest) {
    StartNode* start = new StartNode(0, 2);
    EndNode* end = new EndNode(3, 7);
    LocationNode* loc = new LocationNode(start, end);
    size_t alloc_size = sizeof(int);

    MemoryNode* mem = new MemoryNode(loc, alloc_size);

    ASSERT_NO_THROW(delete mem);
}

TEST(ASTMemoryNode, MemoryHandlesNullLocation) {
    size_t allocationSize = sizeof(int);

    // Construct MemoryNode with null location
    MemoryNode* memoryNode = nullptr;
    ASSERT_NO_THROW(memoryNode = new MemoryNode(nullptr, allocationSize));

    // Verify memory allocation still succeeds
    ASSERT_NE(memoryNode->address, nullptr);

    delete memoryNode;  // Cleanup
}

TEST(ASTMemoryNode, MemoryHandlesZeroSizeAllocation) {
    StartNode* location = new StartNode(1, 1);

    // Construct MemoryNode with zero size
    MemoryNode* memoryNode = nullptr;
    ASSERT_NO_THROW(memoryNode = new MemoryNode(location, 0));

    // Verify address is still non-null (malloc often returns valid address for
    // zero size)
    ASSERT_NE(memoryNode->address, nullptr);

    delete memoryNode;  // Cleanup
}

/* VARIABLE AND VALUES */

TEST(ASTValueNode, ValueAssignment) {
    StartNode* start = new StartNode(0, 4);
    EndNode* end = new EndNode(123, 32);
    LocationNode* loc = new LocationNode(start, end);
    PrimitiveTypeNode* type = new PrimitiveTypeNode("int!");
    string val = "24";

    ValueNode* value = new ValueNode(val, loc, type);

    ASSERT_EQ(std::stoi(value->value), 24);

    delete value;
}

TEST(ASTVariableNode, VariableNameTest) {
    StartNode* start = new StartNode(0, 2);
    EndNode* end = new EndNode(0, 5);
    StartNode* start2 = new StartNode(2, 3);
    EndNode* end2 = new EndNode(4, 2);
    LocationNode* loc1 = new LocationNode(start, end);
    LocationNode* loc2 = new LocationNode(start2, end2);
    PrimitiveTypeNode* type = new PrimitiveTypeNode("int!");
    ValueNode* value = new ValueNode("32", loc1, type);

    VariableNode* var = new VariableNode("my_var", loc2, value);

    ASSERT_EQ(var->name, "my_var");

    delete var;
}

/* POINTER NODE */
TEST(ASTPointerNode, PointerNodeConstructor) {
    StartNode* start = new StartNode(0, 2);
    EndNode* end = new EndNode(0, 5);
    StartNode* start2 = new StartNode(3, 4);
    EndNode* end2 = new EndNode(4, 2);
    StartNode* start3 = new StartNode(0, 5);
    EndNode* end3 = new EndNode(0, 8);
    StartNode* start4 = new StartNode(0, 7);
    EndNode* end4 = new EndNode(0, 9);
    LocationNode* loc = new LocationNode(start, end);
    LocationNode* loc2 = new LocationNode(start2, end2);
    LocationNode* loc3 = new LocationNode(start3, end3);
    LocationNode* loc4 = new LocationNode(start4, end4);
    PrimitiveTypeNode* type = new PrimitiveTypeNode("int!");
    ValueNode* val = new ValueNode("-12", loc, type);
    MemoryNode* mem = new MemoryNode(loc2, sizeof(int));
    VariableNode* var = new VariableNode("my_var", loc3, val);

    // Create PointerNode
    PointerNode* ptr = new PointerNode(loc4, type, mem, var);

    // Validate initialization
    ASSERT_EQ(ptr->location, loc4);
    ASSERT_EQ(ptr->type, type);
    ASSERT_EQ(ptr->memory, mem);
    ASSERT_EQ(ptr->variable, var);

    // Validate type properties
    ASSERT_EQ(type->GetTypeName(), "int!");

    // Cleanup
    delete ptr;  // Ensure no leaks or crashes during cleanup
}

TEST(ASTPointerNode, DestructorFreesMemory) {
    StartNode* start = new StartNode(0, 2);
    EndNode* end = new EndNode(0, 5);
    StartNode* start2 = new StartNode(3, 4);
    EndNode* end2 = new EndNode(4, 2);
    StartNode* start3 = new StartNode(0, 5);
    EndNode* end3 = new EndNode(0, 8);
    StartNode* start4 = new StartNode(0, 7);
    EndNode* end4 = new EndNode(0, 9);
    LocationNode* loc = new LocationNode(start, end);
    LocationNode* loc2 = new LocationNode(start2, end2);
    LocationNode* loc3 = new LocationNode(start3, end3);
    LocationNode* loc4 = new LocationNode(start4, end4);
    PrimitiveTypeNode* type = new PrimitiveTypeNode("int!");
    ValueNode* val = new ValueNode("-12", loc, type);
    MemoryNode* mem = new MemoryNode(loc2, sizeof(int));
    VariableNode* var = new VariableNode("my_var", loc3, val);

    // Create PointerNode
    PointerNode* ptr = new PointerNode(loc4, type, mem, var);

    // Validate initialization
    ASSERT_NO_THROW(delete ptr);  // Ensure proper cleanup
}

/*  EXPRESSION NODE */

// TEST(ASTExpressionTests, AllocationNodeConstructor) {}

/* BODY NODE  */

// TEST(ASTBodyTests, BodyConstructorAndExpressionAccess) {
//     ASTNode* init = new ProgramInitializationNode(":main");
//     vector<ASTNode*> expressions;
//     expressions.push_back(new
// }
