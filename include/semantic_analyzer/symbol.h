#ifndef SYMBOL_H
#define SYMBOL_H
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class SymbolKind { Variable, Constant, Type, Function };
// `Void` is a function return type meaning "returns nothing" (procedure-like).
// Kept last so the existing enumerators keep their values.
enum class SymbolType { Integer, Char, String, Boolean, UserDefined, Void };

inline std::string symbolKindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Variable: return "Variable";
        case SymbolKind::Constant: return "Constant";
        case SymbolKind::Type: return "Type";
        case SymbolKind::Function: return "Function";
        default: return "Unknown";
    }
}
inline std::string symbolTypeToString(SymbolType type) {
    switch (type) {
        case SymbolType::Integer: return "Integer";
        case SymbolType::Char: return "Char";
        case SymbolType::String: return "String";
        case SymbolType::Boolean: return "Boolean";
        case SymbolType::UserDefined: return "UserDefined";
        case SymbolType::Void: return "Void";
        default: return "Unknown";
    }
}

struct Symbol {
    std::string name;
    std::string typeName; // For user-defined types, store the type name
    SymbolKind kind;
    SymbolType type;

    int paramCount;         // only meaningful for functions
    std::vector<SymbolType> paramTypes; // only meaningful for functions

    int ordinal = 0;

    int address;        // offset within its scope (0, 1, 2, ...)
    int scopeIndex = -1; // for functions: index of the body scope (set during analysis)

    std::vector<std::string> members; // only meaningful for user-defined types

    Symbol(std::string name, SymbolType type) : name(std::move(name)), type(type) {}
    Symbol(std::string name, SymbolType type, std::string typeName) : name(std::move(name)), typeName(std::move(typeName)), type(type) {}
    Symbol() = default;

    static SymbolType getSymbolType(std::string type) {
        if (type == "integer") return SymbolType::Integer;
        if (type == "char") return SymbolType::Char;
        if (type == "string") return SymbolType::String;
        if (type == "boolean") return SymbolType::Boolean;
        if (type == "void") return SymbolType::Void;
        return SymbolType::UserDefined;
    }

};

class SymbolTable {
public:
    SymbolTable();

    int enterScope();             // create and enter a new child of the current scope; returns its index
    void reenterScope(int index); // re-enter an existing scope (for later passes, e.g. code generation)
    void exitScope();             // return to the parent scope; scopes are never destroyed
    int currentScopeIndex() const { return currentScope; }
    int scopeLocalCount(int index) const; // number of variable slots reserved in a scope (params + locals)
    std::vector<std::pair<std::string, int>> globalVariables() const; // scope-0 variables (name, address), sorted by address
    bool removeGlobalVariable(const std::string& name); // erase a scope-0 variable (dead-code elimination); false if absent. Compacts remaining globals' addresses.
    bool removeLocalVariable(int scopeIndex, const std::string& name); // erase a variable from a function scope, compact higher local addresses down, and shrink the scope's slot count. Parameters (lowest addresses) are preserved.
    bool declare(const Symbol& sym);   // false if already declared in current scope
    Symbol* lookup(const std::string& name);  // walks up scopes, nullptr if not found
    void printCurrentScope(); // For debugging purposes
    void printAllScopes(); // For debugging purposes
    std::vector<std::pair<Symbol, int>> getAllVariables(); // returns all variables in the current scope and its ancestors, along with their scope indices
private:
    struct Scope {
        std::unordered_map<std::string, Symbol> symbols;
        int addressCounter = 0;   // resets for each scope
        int parent = -1;          // index of the enclosing scope (-1 for the global scope)
    };
    std::vector<Scope> scopes;
    int currentScope = 0;         // index of the active scope (the global scope is 0)

};

#endif // SYMBOL_H