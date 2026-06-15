1. Program Body is missing
2. Program first name and end name do not match
3. Const Value already declared in this scope
4. Variables with custom user defined types need to have already declared types
5. Types are considered as list of literals (Assume that they are ordered list of literals)
7. Type members also need to be declared before they are used
8. Variables also need to be declared before they are used
9. Scopes Management
    i. Variables declared in outer scopes should be accessible in inner scopes
    ii. Variables declared in inner scopes should not be accessible in outer scopes
    iii. Same variable name can be declared in different scopes without conflict (shadowing)
10. Condition in 'if' statement must be of boolean type
11. Type mismatch in relational expression. Left operand has type 'X', but right operand has type 'Y'.
12. Assumption of type members are not supported for +,-,*,/ operations eventhough they are ordered list of literals.
13. Some Functions may not have return type, so we will allow the return type to be optional and default to 'void' (which we will represent as SymbolType::Unknown)
14. Predeclared boolean literals 'true' and 'false' should be treated as constants of type boolean.
15. if there is a variable which is not already declared but assigned from a function call, we declare it with the return type of the function.
16. Allowed +/- for the user defined ordered list of literals if the type members are all integers. Not allowed for boolean types.