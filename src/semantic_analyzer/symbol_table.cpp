#include "semantic_analyzer/symbol.h"

SymbolTable::SymbolTable() {
    // Start with a global scope
    scopes.emplace_back();
}

void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (!scopes.empty()) {
        scopes.pop_back();
    }
}

bool SymbolTable::declare(const Symbol& sym) {
    if (scopes.empty()) {
        return false; // No scope to declare in
    }
    auto& currentScope = scopes.back();
    if (currentScope.find(sym.name) != currentScope.end()) {
        return false; // Symbol already declared in current scope
    }
    currentScope[sym.name] = sym;
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr; // Not found in any scope
}

void SymbolTable::printCurrentScope() {
    if (scopes.empty()) {
        std::cout << "No scopes available." << std::endl;
        return;
    }
    const auto& currentScope = scopes.back();
    std::cout << "Current Scope:" << std::endl;
    for (const auto& pair : currentScope) {
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
        std::cout << "Scope " << i << ":" << std::endl;
        for (const auto& pair : scopes[i]) {
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
}