#include "semantic_analyzer/symbol.h"

#include <algorithm>

SymbolTable::SymbolTable() {
    // Start with a global scope (index 0, no parent).
    scopes.emplace_back();
    currentScope = 0;

    // Predeclare the built-in boolean literals so they resolve like constants.
    Symbol falseSym("false", SymbolType::Boolean);
    falseSym.kind = SymbolKind::Constant;
    falseSym.ordinal = 0;
    declare(falseSym);
    Symbol trueSym("true", SymbolType::Boolean);
    trueSym.kind = SymbolKind::Constant;
    trueSym.ordinal = 1;
    declare(trueSym);
}

int SymbolTable::enterScope() {
    int parent = currentScope;
    scopes.emplace_back();
    int index = static_cast<int>(scopes.size()) - 1;
    scopes[index].parent = parent;
    currentScope = index;
    return index;
}

void SymbolTable::reenterScope(int index) {
    if (index >= 0 && index < static_cast<int>(scopes.size())) {
        currentScope = index;
    }
}

void SymbolTable::exitScope() {
    // Scopes are never destroyed; just move back to the enclosing scope.
    if (currentScope >= 0 && scopes[currentScope].parent >= 0) {
        currentScope = scopes[currentScope].parent;
    }
}

int SymbolTable::scopeLocalCount(int index) const {
    if (index < 0 || index >= static_cast<int>(scopes.size())) return 0;
    return scopes[index].addressCounter;
}

std::vector<std::pair<std::string, int>> SymbolTable::globalVariables() const {
    // Scope 0 is the program (global) scope. Return its variables (name, address),
    // ordered by address so the emitted .data section reads top-to-bottom.
    std::vector<std::pair<std::string, int>> result;
    if (scopes.empty()) return result;
    for (const auto& entry : scopes[0].symbols) {
        if (entry.second.kind == SymbolKind::Variable) {
            result.emplace_back(entry.first, entry.second.address);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    return result;
}

bool SymbolTable::declare(const Symbol& sym) {
    if (scopes.empty() || currentScope < 0) return false; // No scope to declare in

    auto& scope = scopes[currentScope];
    if (scope.symbols.count(sym.name)) return false; // Already declared in current scope
    Symbol s = sym;
    if (s.kind == SymbolKind::Variable) {
        s.address = scope.addressCounter++;
        // Record the owning scope so code generation can tell a global (scope 0,
        // absolute addressing) from a function-local (frame-relative addressing).
        s.scopeIndex = currentScope;
    } else {
        s.address = -1;
    }
    scope.symbols[sym.name] = s;
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    // Walk from the current scope up the chain of parents.
    for (int index = currentScope; index >= 0; index = scopes[index].parent) {
        auto found = scopes[index].symbols.find(name);
        if (found != scopes[index].symbols.end()) {
            return &found->second;
        }
    }
    return nullptr; // Not found in any enclosing scope
}

void SymbolTable::printCurrentScope() {
    if (scopes.empty() || currentScope < 0) {
        std::cout << "No scopes available." << std::endl;
        return;
    }
    const auto& scope = scopes[currentScope];
    std::cout << "Current Scope:" << std::endl;
    for (const auto& pair : scope.symbols) {
        const Symbol& sym = pair.second;
        std::cout << "  Name: " << sym.name << ", Kind: " << static_cast<int>(sym.kind)
                  << ", Type: " << static_cast<int>(sym.type);
        if (sym.kind == SymbolKind::Type && !sym.members.empty()) {
            std::cout << ", Members: [";
            for (const auto& member : sym.members) {
                std::cout << member << " ";
            }
            std::cout << "]";
        }
        std::cout << std::endl;
    }
}

void SymbolTable::printAllScopes() {
    for (size_t i = 0; i < scopes.size(); ++i) {
        std::cout << "Scope " << i << " (parent " << scopes[i].parent << "):" << std::endl;
        for (const auto& pair : scopes[i].symbols) {
            const Symbol& sym = pair.second;
            std::cout << "  Name: " << sym.name << ", Kind: " << static_cast<int>(sym.kind)
                      << ", Type: " << static_cast<int>(sym.type);
            if (sym.kind == SymbolKind::Type && !sym.members.empty()) {
                std::cout << ", Members: [";
                for (const auto& member : sym.members) {
                    std::cout << member << " ";
                }
                std::cout << "]";
            }
            if (sym.kind == SymbolKind::Constant){
                std::cout << ", Ordinal: " << sym.ordinal;
            }
            if (sym.kind == SymbolKind::Variable){
                std::cout << ", Address: " << sym.address;
            }
            if (sym.kind == SymbolKind::Function){
                std::cout << ", BodyScope: " << sym.scopeIndex;
            }
            std::cout << std::endl;
        }
    }
}
