// tests/unit/test_symbol_table.cpp
//
// Unit tests for SymbolTable, the scope/symbol store used by the semantic
// analyzer (and re-used by code generation). Exercises declaration, lookup,
// nested-scope visibility, shadowing, and scope navigation directly, without
// going through the analyzer.
#include <gtest/gtest.h>

#include <iostream>
#include <sstream>

#include "semantic_analyzer/symbol.h"

namespace {

Symbol makeVar(const std::string& name, SymbolType type) {
    Symbol s(name, type);
    s.kind = SymbolKind::Variable;
    return s;
}

}  // namespace

TEST(SymbolTableTest, PredeclaresBooleanLiterals) {
    SymbolTable table;
    Symbol* t = table.lookup("true");
    Symbol* f = table.lookup("false");
    ASSERT_NE(t, nullptr);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(t->type, SymbolType::Boolean);
    EXPECT_EQ(f->type, SymbolType::Boolean);
    EXPECT_EQ(t->ordinal, 1);
    EXPECT_EQ(f->ordinal, 0);
}

TEST(SymbolTableTest, DeclareThenLookupSucceeds) {
    SymbolTable table;
    EXPECT_TRUE(table.declare(makeVar("x", SymbolType::Integer)));
    Symbol* x = table.lookup("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->type, SymbolType::Integer);
}

TEST(SymbolTableTest, DuplicateDeclarationInSameScopeFails) {
    SymbolTable table;
    EXPECT_TRUE(table.declare(makeVar("x", SymbolType::Integer)));
    EXPECT_FALSE(table.declare(makeVar("x", SymbolType::Char)));
    // The original declaration is retained.
    EXPECT_EQ(table.lookup("x")->type, SymbolType::Integer);
}

TEST(SymbolTableTest, AssignsIncrementingAddressesToVariables) {
    SymbolTable table;
    table.declare(makeVar("a", SymbolType::Integer));
    table.declare(makeVar("b", SymbolType::Integer));
    EXPECT_EQ(table.lookup("a")->address, 0);
    EXPECT_EQ(table.lookup("b")->address, 1);
}

TEST(SymbolTableTest, InnerScopeSeesOuterDeclarations) {
    SymbolTable table;
    table.declare(makeVar("outer", SymbolType::Integer));
    table.enterScope();
    EXPECT_NE(table.lookup("outer"), nullptr);  // visible from inner scope
    table.exitScope();
}

TEST(SymbolTableTest, OuterScopeCannotSeeInnerDeclarations) {
    SymbolTable table;
    int inner = table.enterScope();
    table.declare(makeVar("local", SymbolType::Integer));
    EXPECT_NE(table.lookup("local"), nullptr);
    table.exitScope();
    EXPECT_EQ(table.lookup("local"), nullptr);  // not visible after leaving
    EXPECT_GT(inner, 0);
}

TEST(SymbolTableTest, ShadowingResolvesToNearestScope) {
    SymbolTable table;
    table.declare(makeVar("v", SymbolType::Integer));
    table.enterScope();
    table.declare(makeVar("v", SymbolType::Char));
    EXPECT_EQ(table.lookup("v")->type, SymbolType::Char);  // inner shadows outer
    table.exitScope();
    EXPECT_EQ(table.lookup("v")->type, SymbolType::Integer);  // outer again
}

TEST(SymbolTableTest, EnterAndExitScopeTrackCurrentIndex) {
    SymbolTable table;
    EXPECT_EQ(table.currentScopeIndex(), 0);
    int child = table.enterScope();
    EXPECT_EQ(table.currentScopeIndex(), child);
    table.exitScope();
    EXPECT_EQ(table.currentScopeIndex(), 0);
}

TEST(SymbolTableTest, ReenterScopeReturnsToAPreviouslyCreatedScope) {
    SymbolTable table;
    int child = table.enterScope();
    table.declare(makeVar("inner", SymbolType::Integer));
    table.exitScope();
    EXPECT_EQ(table.lookup("inner"), nullptr);

    table.reenterScope(child);
    EXPECT_EQ(table.currentScopeIndex(), child);
    EXPECT_NE(table.lookup("inner"), nullptr);  // re-entering restores visibility

    // Out-of-range indices are ignored (no change to the current scope).
    table.reenterScope(999);
    EXPECT_EQ(table.currentScopeIndex(), child);
}

TEST(SymbolTableTest, VariablesRecordTheirOwningScope) {
    SymbolTable table;
    table.declare(makeVar("g", SymbolType::Integer));  // global scope (0)
    EXPECT_EQ(table.lookup("g")->scopeIndex, 0);

    int inner = table.enterScope();
    table.declare(makeVar("local", SymbolType::Integer));
    EXPECT_EQ(table.lookup("local")->scopeIndex, inner);  // distinguishes locals from globals
    table.exitScope();
}

TEST(SymbolTableTest, ScopeLocalCountTracksReservedSlots) {
    SymbolTable table;
    table.declare(makeVar("a", SymbolType::Integer));
    table.declare(makeVar("b", SymbolType::Integer));

    int fn = table.enterScope();           // a function's body scope
    table.declare(makeVar("p", SymbolType::Integer));  // parameter -> slot 0
    table.declare(makeVar("v", SymbolType::Integer));  // local     -> slot 1
    table.exitScope();

    EXPECT_EQ(table.scopeLocalCount(0), 2);    // two globals
    EXPECT_EQ(table.scopeLocalCount(fn), 2);   // param + local
    EXPECT_EQ(table.scopeLocalCount(999), 0);  // out-of-range is harmless
}

TEST(SymbolTableTest, PrintHelpersRunWithoutCrashing) {
    SymbolTable table;
    Symbol type("Color", SymbolType::UserDefined);
    type.kind = SymbolKind::Type;
    type.members = {"red", "green"};
    table.declare(type);
    table.declare(makeVar("x", SymbolType::Integer));

    std::ostringstream sink;
    std::streambuf* previous = std::cout.rdbuf(sink.rdbuf());
    table.printCurrentScope();
    table.printAllScopes();
    std::cout.rdbuf(previous);

    EXPECT_FALSE(sink.str().empty());
}
