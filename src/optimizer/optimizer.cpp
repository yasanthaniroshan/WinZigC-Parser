#include "optimizer/optimizer.h"
#include <iostream>
#include <unordered_set>

Result<TreeNode *> Optimizer::preOptimize()
{
    // O0: emit exactly what the front end produced.
    if (optimizationLevel == "O0")
    {
        return Result<TreeNode *>::Ok(ast);
    }
    const bool isO1 = (optimizationLevel == "O1");
    const bool isO2 = (optimizationLevel == "O2");
    if (!isO1 && !isO2)
    {
        return Result<TreeNode *>::Ok(ast); // unknown level: leave the tree untouched
    }

    TreeNode *name = ast->left;        // First child is program name
    TreeNode *consts = name->right;    // First sibling is const declarations
    TreeNode *types = consts->right;   // Next sibling is type declarations
    TreeNode *dclns = types->right;    // Next sibling is variable and function declarations
    TreeNode *subprogs = dclns->right; // Next sibling is subprogram declarations
    TreeNode *body = subprogs->right;  // Next sibling is the body of the program

    // ---- O1 (and O2): dead-code elimination ----
    // Order: unused globals, then unused locals, then unused functions.
    auto rGlobals = removeUnusedVariables(dclns, subprogs, body);
    if (rGlobals.isErr())
        return Result<TreeNode *>::Err(OptimizerError(rGlobals.error_message.value()));
    auto rLocals = removeUnusedLocalVariables(subprogs);
    if (rLocals.isErr())
        return Result<TreeNode *>::Err(OptimizerError(rLocals.error_message.value()));
    auto rFunctions = removeUnusedFunctions(subprogs, body);
    if (rFunctions.isErr())
        return Result<TreeNode *>::Err(OptimizerError(rFunctions.error_message.value()));

    // ---- O2 only: constant propagation + constant folding ----
    if (isO2)
    {
        // Globals eligible for propagation: confined to the main body (not touched by
        // any surviving subprogram), so the single-definition analysis is sound.
        std::unordered_set<std::string> eligibleGlobals;
        for (const auto &g : collectDeclaredNames(dclns))
            if (!subtreeReferences(subprogs, g))
                eligibleGlobals.insert(g);

        // Named constants are immutable, so resolve them to literals once up front;
        // the fixpoint loop below then folds/branches on the resulting values.
        auto named = propagateNamedConstants(body);
        if (named.isErr())
            return Result<TreeNode *>::Err(OptimizerError(named.error_message.value()));

        int numberOfPasses = 0;
        while (numberOfPasses < MAX_PASSES)
        {
            instructionRemovedCount = 0;
            // Propagate single-assignment constants first: this turns expressions like
            // `x + 3` (x assigned a constant) into foldable `5 + 3`, which the folding
            // pass then collapses. Running both to a fixpoint chains the two together.
            auto pGlobals = propagateConstants(body, eligibleGlobals);
            if (pGlobals.isErr())
                return Result<TreeNode *>::Err(OptimizerError(pGlobals.error_message.value()));
            auto pLocals = propagateConstantsInFunctions(subprogs);
            if (pLocals.isErr())
                return Result<TreeNode *>::Err(OptimizerError(pLocals.error_message.value()));
            auto pCopies = propagateCopies(body, eligibleGlobals);
            if (pCopies.isErr())
                return Result<TreeNode *>::Err(OptimizerError(pCopies.error_message.value()));
            auto folded = constantFoldingPass();
            if (folded.isErr())
                return Result<TreeNode *>::Err(OptimizerError(folded.error_message.value()));
            // Now that conditions may have folded to constants, prune dead branches,
            // then drop any code left unreachable after a return/exit.
            eliminateDeadBranches(ast);
            removeUnreachableCode(ast);
            LOG_DEBUG("instructions removed: " + std::to_string(instructionRemovedCount));
            if (instructionRemovedCount == 0)
                break; // fixpoint reached
            numberOfPasses++;
        }

        // Propagation can leave globals/locals with no remaining references; sweep them.
        auto cGlobals = removeUnusedVariables(dclns, subprogs, body);
        if (cGlobals.isErr())
            return Result<TreeNode *>::Err(OptimizerError(cGlobals.error_message.value()));
        auto cLocals = removeUnusedLocalVariables(subprogs);
        if (cLocals.isErr())
            return Result<TreeNode *>::Err(OptimizerError(cLocals.error_message.value()));
    }

    if (!warnings.empty())
    {
        std::cerr << "\n";
        for (const auto &warning : warnings)
        {
            diagnostics::warning(warning.msg, warning.line, warning.column);
        }
        diagnostics::summary("Optimization completed with " + std::to_string(warnings.size()) + " warning(s).",
                             diagnostics::Severity::Warning);
    }
    return Result<TreeNode *>::Ok(ast);
}

//===== Peephole optimization (post-codegen) =====

namespace {

// One .text instruction with any labels that immediately precede it.
struct AsmInstr {
    std::vector<std::string> labels;  // labels attached to this instruction (may be empty)
    std::string op;                   // opcode
    std::string operand;              // operand text (may be empty)
};

std::string trimLine(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

bool labelsContain(const std::vector<std::string> &labels, const std::string &name) {
    for (const auto &l : labels)
        if (l == name) return true;
    return false;
}

// True if the pair (a then b) computes a net stack no-op and can be deleted.
bool pairCancels(const AsmInstr &a, const AsmInstr &b) {
    // x + 0 = x, x - 0 = x
    if (a.op == "lit" && a.operand == "0" && (b.op == "add" || b.op == "subtract"))
        return true;
    // x * 1 = x, x / 1 = x (integer division by 1 is exact)
    if (a.op == "lit" && a.operand == "1" && (b.op == "multiply" || b.op == "divide"))
        return true;
    // -(-x) = x
    if (a.op == "negate" && b.op == "negate")
        return true;
    // load v; save v -> self-copy (push v, then pop and store back into v): no-op.
    // (The reverse, save v; load v, is NOT removable -- save has a side effect.)
    if (((a.op == "load" && b.op == "save") ||
         (a.op == "load_local" && b.op == "save_local")) &&
        !a.operand.empty() && a.operand == b.operand)
        return true;
    return false;
}

// One peephole sweep. Returns the number of instructions removed.
int peepholeOnce(std::vector<AsmInstr> &units, std::vector<std::string> &trailingLabels) {
    std::vector<AsmInstr> out;
    out.reserve(units.size());
    std::vector<std::string> carry;  // labels to attach to the next emitted instruction
    int removed = 0;

    size_t i = 0;
    while (i < units.size()) {
        AsmInstr cur = units[i];
        if (!carry.empty()) {
            cur.labels.insert(cur.labels.begin(), carry.begin(), carry.end());
            carry.clear();
        }

        if (i + 1 < units.size()) {
            const AsmInstr &nxt = units[i + 1];
            // Pair cancellation: second instruction must not be a jump target.
            if (nxt.labels.empty() && pairCancels(cur, nxt)) {
                carry = cur.labels;  // both removed; cur's labels float forward
                i += 2;
                removed += 2;
                continue;
            }
            // `goto L` where L labels the very next instruction: the jump just falls
            // through. Drop the goto; its labels float onto the next instruction.
            if (cur.op == "goto" && labelsContain(nxt.labels, cur.operand)) {
                carry = cur.labels;
                i += 1;
                removed += 1;
                continue;
            }
        }

        out.push_back(std::move(cur));
        ++i;
    }

    // Any labels left with no following instruction land on the program-end position
    if (!carry.empty())
        trailingLabels.insert(trailingLabels.end(), carry.begin(), carry.end());

    units = std::move(out);
    return removed;
}

}  // namespace

Result<void> Optimizer::postOptimize(std::vector<std::string> &asmLines)
{
    // Peephole is an O2-level cleanup; O0 and O1 emit codegen output untouched.
    if (optimizationLevel != "O2")
        return Result<void>::Ok();

    // Locate the start of the code body: everything up to and including `.globl main`
    // (the .data/.rodata/.text headers) is preserved verbatim.
    size_t bodyStart = asmLines.size();
    for (size_t i = 0; i < asmLines.size(); ++i) {
        if (trimLine(asmLines[i]) == ".globl main") {
            bodyStart = i + 1;
            break;
        }
    }
    if (bodyStart >= asmLines.size())
        return Result<void>::Ok();  // unexpected format: leave it alone

    // Parse the .text body into (labels, instruction) units, dropping cosmetic blanks.
    std::vector<AsmInstr> units;
    std::vector<std::string> pendingLabels;
    std::vector<std::string> trailingLabels;
    for (size_t i = bodyStart; i < asmLines.size(); ++i) {
        std::string t = trimLine(asmLines[i]);
        if (t.empty())
            continue;
        if (t.back() == ':') {
            pendingLabels.push_back(t.substr(0, t.size() - 1));
            continue;
        }
        AsmInstr instr;
        instr.labels = std::move(pendingLabels);
        pendingLabels.clear();
        size_t sp = t.find(' ');
        if (sp == std::string::npos) {
            instr.op = t;
        } else {
            instr.op = t.substr(0, sp);
            instr.operand = t.substr(sp + 1);
        }
        units.push_back(std::move(instr));
    }
    // Labels with no following instruction (rare) are preserved at the program end.
    trailingLabels = std::move(pendingLabels);

    // Sweep to a fixpoint: one removal can expose another.
    int totalRemoved = 0;
    int passes = 0;
    while (passes < MAX_PASSES) {
        int removed = peepholeOnce(units, trailingLabels);
        totalRemoved += removed;
        if (removed == 0)
            break;
        ++passes;
    }
    instructionRemovedCount = totalRemoved;
    LOG_DEBUG("peephole removed " + std::to_string(totalRemoved) + " instruction(s)");

    if (totalRemoved == 0)
        return Result<void>::Ok();

    // Re-emit: keep the header verbatim, then write each unit's labels followed by
    // its tab-indented instruction (matching assembleSectioned's surface form).
    std::vector<std::string> rebuilt(asmLines.begin(), asmLines.begin() + bodyStart);
    for (const auto &u : units) {
        for (const auto &lbl : u.labels) {
            rebuilt.push_back("");
            rebuilt.push_back(lbl + ":");
        }
        rebuilt.push_back("\t" + (u.operand.empty() ? u.op : u.op + " " + u.operand));
    }
    for (const auto &lbl : trailingLabels) {
        rebuilt.push_back("");
        rebuilt.push_back(lbl + ":");
    }
    asmLines = std::move(rebuilt);
    return Result<void>::Ok();
}

namespace {

bool isIntLiteral(TreeNode *n) { return n && n->value == "<integer>" && n->left; }
long intValue(TreeNode *n) { return std::stol(n->left->value); }

// True if `node`'s subtree contains a function call (a possible side effect), so we
// must not drop it during algebraic simplification (e.g. f() * 0 must still call f).
bool containsCall(TreeNode *node) {
    if (!node) return false;
    if (node->value == "call") return true;
    for (TreeNode *c = node->left; c; c = c->right)
        if (containsCall(c)) return true;
    return false;
}

}  // namespace

TreeNode *Optimizer::makeIntNode(long value, TreeNode *at)
{
    TreeNode *n = new TreeNode("<integer>", at ? at->line : -1, at ? at->column : -1);
    n->left = new TreeNode(std::to_string(value), at ? at->line : -1, at ? at->column : -1);
    return n;
}

// Fold/simplify a single (already child-simplified) expression node. Returns the
// node to use in its place: a new <integer> when folded, an existing operand when an
// algebraic identity applies, or `node` unchanged. Per WinZigC, relational and
// and/or/not operators yield booleans, represented here as the integers 0 and 1.
TreeNode *Optimizer::foldExprNode(TreeNode *node)
{
    const std::string &op = node->value;
    TreeNode *lhs = node->left;

    // Unary operators: '-' / 'not' with a single operand.
    if ((op == "-" || op == "not") && lhs && lhs->right == nullptr)
    {
        if (!isIntLiteral(lhs)) return node;
        long a = intValue(lhs);
        instructionRemovedCount++;
        return makeIntNode(op == "-" ? -a : (a == 0 ? 1 : 0), node);
    }

    // Binary operators.
    static const std::unordered_set<std::string> binOps = {
        "+", "-", "*", "/", "mod", "<", "<=", ">", ">=", "=", "<>", "and", "or"};
    if (!binOps.count(op) || !lhs) return node;
    TreeNode *rhs = lhs->right;
    if (!rhs) return node;

    if (isIntLiteral(lhs) && isIntLiteral(rhs))
    {
        long a = intValue(lhs), b = intValue(rhs), r = 0;
        // Division/modulus: only fold non-negative operands, where the result is the
        // same under every rounding convention. Leave the rest (and divide-by-zero)
        // to the machine so the optimizer never changes a program's value.
        if ((op == "/" || op == "mod") && !(a >= 0 && b > 0)) return node;
        if (op == "+") r = a + b;
        else if (op == "-") r = a - b;
        else if (op == "*") r = a * b;
        else if (op == "/") r = a / b;
        else if (op == "mod") r = a % b;
        else if (op == "<") r = (a < b) ? 1 : 0;
        else if (op == "<=") r = (a <= b) ? 1 : 0;
        else if (op == ">") r = (a > b) ? 1 : 0;
        else if (op == ">=") r = (a >= b) ? 1 : 0;
        else if (op == "=") r = (a == b) ? 1 : 0;
        else if (op == "<>") r = (a != b) ? 1 : 0;
        else if (op == "and") r = (a != 0 && b != 0) ? 1 : 0;
        else if (op == "or") r = (a != 0 || b != 0) ? 1 : 0;
        instructionRemovedCount++;
        return makeIntNode(r, node);
    }

    // Algebraic identities with exactly one constant operand. Identities that KEEP
    // the non-constant operand are always safe (we drop a literal); identities that
    // DROP it (x*0) are only safe when that operand has no side effects.
    bool lz = isIntLiteral(lhs) && intValue(lhs) == 0;
    bool rz = isIntLiteral(rhs) && intValue(rhs) == 0;
    bool l1 = isIntLiteral(lhs) && intValue(lhs) == 1;
    bool r1 = isIntLiteral(rhs) && intValue(rhs) == 1;
    auto keep = [&](TreeNode *operand) { instructionRemovedCount++; return operand; };

    if (op == "+" && rz) return keep(lhs);
    if (op == "+" && lz) return keep(rhs);
    if (op == "-" && rz) return keep(lhs);
    if (op == "*" && r1) return keep(lhs);
    if (op == "*" && l1) return keep(rhs);
    if (op == "/" && r1) return keep(lhs);
    if (op == "*" && rz && !containsCall(lhs)) { instructionRemovedCount++; return makeIntNode(0, node); }
    if (op == "*" && lz && !containsCall(rhs)) { instructionRemovedCount++; return makeIntNode(0, node); }
    return node;
}

// Post-order recursive simplification: fold each child subtree, then this node.
// `node`'s own right sibling is never touched (the caller relinks it).
TreeNode *Optimizer::simplifyExpr(TreeNode *node)
{
    if (!node) return nullptr;
    TreeNode *first = nullptr, *prev = nullptr;
    for (TreeNode *child = node->left; child != nullptr;)
    {
        TreeNode *nextSib = child->right;
        TreeNode *folded = simplifyExpr(child);
        folded->right = nullptr;
        if (!first) first = folded;
        else prev->right = folded;
        prev = folded;
        child = nextSib;
    }
    node->left = first;
    return foldExprNode(node);
}

Result<void> Optimizer::constantFoldingPass()
{
    ast = simplifyExpr(ast); // the program node never folds, but its subtree does
    return Result<void>::Ok();
}

// Replace `<identifier>(c)` reads with `<integer>(ordinal)` for global constants
// (covers user consts like `Max`, the built-in `true`/`false`, and enum literals).
// Restricted to the main body, where a constant name is unambiguous: a global const
// and a global variable can't share a name, so there is no shadowing to worry about.
void Optimizer::replaceConstIdentifiers(TreeNode *parent)
{
    if (!parent) return;
    TreeNode *prev = nullptr;
    for (TreeNode *c = parent->left; c != nullptr;)
    {
        if (c->value == "<identifier>" && c->left)
        {
            Symbol *sym = symbolTable.lookup(c->left->value);
            if (sym && sym->kind == SymbolKind::Constant &&
                (sym->type == SymbolType::Integer || sym->type == SymbolType::Boolean ||
                 sym->type == SymbolType::UserDefined)) // not Char/String (their literals differ)
            {
                TreeNode *lit = makeIntNode(sym->ordinal, c);
                lit->right = c->right;
                if (!prev) parent->left = lit;
                else prev->right = lit;
                instructionRemovedCount++;
                prev = lit;
                c = lit->right;
                continue;
            }
        }
        replaceConstIdentifiers(c);
        prev = c;
        c = c->right;
    }
}

Result<void> Optimizer::propagateNamedConstants(TreeNode *body)
{
    replaceConstIdentifiers(body);
    return Result<void>::Ok();
}

// If `node` is an `if`/`while` whose condition has folded to a constant, return the
// statement that survives (or nullptr to drop it); otherwise return `node` unchanged.
TreeNode *Optimizer::reduceBranch(TreeNode *node)
{
    if (!node) return nullptr;
    if (node->value == "if")
    {
        TreeNode *cond = node->left;
        if (!isIntLiteral(cond)) return node;
        long v = intValue(cond);
        TreeNode *thenStmt = cond->right;
        TreeNode *elseStmt = thenStmt ? thenStmt->right : nullptr;
        instructionRemovedCount++;
        if (v != 0) return thenStmt;            // condition always true  -> keep `then`
        return elseStmt ? elseStmt : nullptr;   // always false -> keep `else`, or drop
    }
    if (node->value == "while")
    {
        TreeNode *cond = node->left;
        if (isIntLiteral(cond) && intValue(cond) == 0) // `while false` never runs
        {
            instructionRemovedCount++;
            return nullptr;
        }
        // `while true` is an intentional infinite loop; leave it alone.
    }
    return node;
}

// Recursively eliminate dead branches throughout the tree. Children are processed
// first (post-order), so a surviving branch is already simplified when it is hoisted
// into its parent's statement list.
TreeNode *Optimizer::eliminateDeadBranches(TreeNode *node)
{
    if (!node) return nullptr;
    TreeNode *first = nullptr, *prev = nullptr;
    for (TreeNode *child = node->left; child != nullptr;)
    {
        TreeNode *nextSib = child->right;
        TreeNode *reduced = reduceBranch(eliminateDeadBranches(child));
        if (reduced)
        {
            reduced->right = nullptr;
            if (!first) first = reduced;
            else prev->right = reduced;
            prev = reduced;
        }
        child = nextSib;
    }
    node->left = first;
    return node;
}

// Drop statements that follow an unconditional `return`/`exit` in a statement list.
// Only `block` nodes are true sequential statement lists; for other nodes (if/while/
// case condition+arms) the children are NOT sequential, so we recurse without pruning.
TreeNode *Optimizer::removeUnreachableCode(TreeNode *node)
{
    if (!node) return nullptr;
    const bool isBlock = (node->value == "block");
    TreeNode *first = nullptr, *prev = nullptr;
    bool terminated = false;
    for (TreeNode *child = node->left; child != nullptr;)
    {
        TreeNode *nextSib = child->right;
        if (isBlock && terminated)
        {
            instructionRemovedCount++; // statement is unreachable: drop it
            child = nextSib;
            continue;
        }
        TreeNode *processed = removeUnreachableCode(child);
        processed->right = nullptr;
        if (!first) first = processed;
        else prev->right = processed;
        prev = processed;
        if (isBlock && (child->value == "return" || child->value == "exit"))
            terminated = true;
        child = nextSib;
    }
    node->left = first;
    return node;
}

// Replace `<identifier>(from)` reads with `<identifier>(to)` in `parent`'s subtree.
int Optimizer::replaceReadsWithIdentifier(TreeNode *parent, const std::string &from, const std::string &to)
{
    if (!parent) return 0;
    int replaced = 0;
    TreeNode *prev = nullptr;
    for (TreeNode *c = parent->left; c != nullptr;)
    {
        if (c->value == "<identifier>" && c->left && c->left->value == from)
        {
            TreeNode *id = new TreeNode("<identifier>", c->line, c->column);
            id->left = new TreeNode(to, c->line, c->column);
            id->right = c->right;
            if (!prev) parent->left = id;
            else prev->right = id;
            replaced++;
            prev = id;
            c = id->right;
        }
        else
        {
            replaced += replaceReadsWithIdentifier(c, from, to);
            prev = c;
            c = c->right;
        }
    }
    return replaced;
}

// Copy propagation: for a single-assignment `x := y` (both confined to the body), if
// `y` is never reassigned from the copy onward, rewrite later reads of `x` to `y` and
// drop the copy. Same dominance conditions as constant propagation, plus the "y stable"
// requirement so every use of x sees the same value y held at the copy.
Result<void> Optimizer::propagateCopies(TreeNode *body, const std::unordered_set<std::string> &eligible)
{
    if (!body || !body->left) return Result<void>::Ok();
    TreeNode *prevStmt = nullptr;
    for (TreeNode *stmt = body->left; stmt != nullptr;)
    {
        TreeNode *nextStmt = stmt->right;
        bool removed = false;

        if (stmt->value == "assign" && stmt->left && stmt->left->value == "<identifier>" &&
            stmt->left->left)
        {
            const std::string x = stmt->left->left->value;
            TreeNode *rhs = stmt->left->right;
            if (eligible.count(x) && rhs && rhs->value == "<identifier>" && rhs->left)
            {
                const std::string y = rhs->left->value;
                if (x != y && eligible.count(y) && countVariableWrites(body, x) == 1)
                {
                    bool referencedBefore = false;
                    for (TreeNode *s = body->left; s != stmt; s = s->right)
                        if (subtreeReferences(s, x)) { referencedBefore = true; break; }

                    bool yStable = true; // y must not be reassigned at or after the copy
                    for (TreeNode *s = stmt; s != nullptr; s = s->right)
                        if (countVariableWrites(s, y) > 0) { yStable = false; break; }

                    if (!referencedBefore && yStable)
                    {
                        for (TreeNode *s = nextStmt; s != nullptr; s = s->right)
                            replaceReadsWithIdentifier(s, x, y);
                        if (!prevStmt) body->left = nextStmt;
                        else prevStmt->right = nextStmt;
                        instructionRemovedCount++;
                        removed = true;
                    }
                }
            }
        }

        if (!removed) prevStmt = stmt;
        stmt = nextStmt;
    }
    return Result<void>::Ok();
}

bool Optimizer::isVariableUsed(TreeNode *subtreeRoot, const std::string &name)
{
    if (!subtreeRoot) return false;
    treeTraveler.setSubtreeRoot(subtreeRoot);
    treeTraveler.setNodeValue(name);
    while (TreeNode *node = treeTraveler.step())
    {
        if (node->value == name) return true;
    }
    return false;
}

TreeNode* Optimizer::spliceDeclaration(TreeNode *dclns, const std::string &name)
{
    if (!dclns) return nullptr;
    TreeNode *prevVar = nullptr;
    for (TreeNode *var = dclns->left; var != nullptr; prevVar = var, var = var->right)
    {
        if (var->value != "var") continue;
        TreeNode *prevId = nullptr;
        // Stop before the last child (id->right == nullptr): that one is the type, not a name.
        for (TreeNode *id = var->left; id != nullptr && id->right != nullptr; prevId = id, id = id->right)
        {
            if (!id->left || id->left->value != name) continue;
            // Unlink this name identifier from the var node's child chain.
            if (prevId == nullptr)
                var->left = id->right;
            else
                prevId->right = id->right;
            // If only the type identifier remains, the var node declares nothing -> remove it.
            if (var->left->right == nullptr)
            {
                if (prevVar == nullptr)
                    dclns->left = var->right;
                else
                    prevVar->right = var->right;
            }
            return var;
        }
    }
    return nullptr;
}

Result<void> Optimizer::removeUnusedVariables(TreeNode *dclns, TreeNode *subprogs, TreeNode *body)
{
    std::vector<std::pair<Symbol, int>> allVariables = symbolTable.getAllVariables();
    for (const auto &[symbol, scopeIndex] : allVariables)
    {
        if (scopeIndex != 0) continue; // Only consider global variables
        // A global is live if referenced in EITHER the subprograms or the body. Deciding on each subtree independently would wrongly flag a body-only variable as dead.
        bool isUsed = isVariableUsed(subprogs, symbol.name) || isVariableUsed(body, symbol.name);
        if (isUsed)
        {
            continue;
        }
        LOG_DEBUG("Removing unused variable: " + symbol.name);
        symbolTable.removeGlobalVariable(symbol.name); // drop from .data
        TreeNode *removeVarNode = spliceDeclaration(dclns, symbol.name);          // drop from the AST
        if (removeVarNode)
        {
            addWarning("Removed unused variable: " + symbol.name, removeVarNode->line, removeVarNode->column);
        }
    }
    return Result<void>::Ok();
}

std::vector<std::string> Optimizer::collectDeclaredNames(TreeNode *dclns)
{
    std::vector<std::string> names;
    if (!dclns) return names;
    for (TreeNode *var = dclns->left; var != nullptr; var = var->right)
    {
        if (var->value != "var") continue;
        // Stop before the last child (id->right == nullptr): that one is the type, not a name.
        for (TreeNode *id = var->left; id != nullptr && id->right != nullptr; id = id->right)
        {
            if (id->left) names.push_back(id->left->value);
        }
    }
    return names;
}

Result<void> Optimizer::removeUnusedLocalVariables(TreeNode *subprogs)
{
    if (!subprogs) return Result<void>::Ok();
    // subprogs -> left is the first function; siblings via ->right. Functions don't nest in
    // this grammar, so each function's locals are checked against its own body only.
    for (TreeNode *fcn = subprogs->left; fcn != nullptr; fcn = fcn->right)
    {
        TreeNode *name = fcn->left; // function name identifier
        if (!name) continue;
        // Layout: name, params, returnType, consts, types, dclns, body, endName.
        TreeNode *params = name->right;
        if (!params) continue;
        TreeNode *returnType = params->right;
        if (!returnType) continue;
        TreeNode *consts = returnType->right;
        if (!consts) continue;
        TreeNode *types = consts->right;
        if (!types) continue;
        TreeNode *dclns = types->right;
        if (!dclns) continue;
        TreeNode *body = dclns->right;
        if (!body) continue;

        // Collect names up front: splicing mutates the dclns chain as we go. Parameters live under `params`, not `dclns`, so they are never considered here (correctly kept).
        std::vector<std::string> localNames = collectDeclaredNames(dclns);
        std::string fcnName = name->left ? name->left->value : "<anonymous>";
        // The function's body scope was anchored on its symbol during analysis; we need it to reclaim local stack slots. -1 means "not found" -> skip the symbol-table compaction.
        Symbol *fcnSym = symbolTable.lookup(fcnName);
        int fcnScope = fcnSym ? fcnSym->scopeIndex : -1;
        for (const auto &localName : localNames)
        {
            if (isVariableUsed(body, localName)) continue;
            LOG_DEBUG("Removing unused local variable '" + localName + "' in function '" + fcnName + "'");
            // Drop the declaration node from the AST and reclaim its frame slot: compact the remaining locals' addresses and shrink the scope's reserve count.
            TreeNode *removed = spliceDeclaration(dclns, localName);
            if (fcnScope >= 0)
            {
                symbolTable.removeLocalVariable(fcnScope, localName);
            }
            if (removed)
            {
                addWarning("Removed unused variable '" + localName + "' in function '" + fcnName + "'",
                           removed->line, removed->column);
            }
        }
    }
    return Result<void>::Ok();
}


int Optimizer::countVariableWrites(TreeNode *node, const std::string &name)
{
    if (!node) return 0;
    int count = 0;
    if (node->value == "assign" && node->left && node->left->value == "<identifier>" &&
        node->left->left && node->left->left->value == name)
    {
        count++;
    }
    else if (node->value == "swap" && node->left)
    {
        TreeNode *l = node->left;
        TreeNode *r = l->right;
        if (l->value == "<identifier>" && l->left && l->left->value == name) count++;
        if (r && r->value == "<identifier>" && r->left && r->left->value == name) count++;
    }
    else if (node->value == "read")
    {
        for (TreeNode *c = node->left; c != nullptr; c = c->right)
            if (c->value == "<identifier>" && c->left && c->left->value == name) count++;
    }
    for (TreeNode *c = node->left; c != nullptr; c = c->right)
        count += countVariableWrites(c, name);
    return count;
}

// Does `name` appear anywhere (read or write) as an identifier in `node`'s subtree?
bool Optimizer::subtreeReferences(TreeNode *node, const std::string &name)
{
    if (!node) return false;
    if (node->value == "<identifier>" && node->left && node->left->value == name) return true;
    for (TreeNode *c = node->left; c != nullptr; c = c->right)
        if (subtreeReferences(c, name)) return true;
    return false;
}

// Replace every `<identifier>` read of `name` in `parent`'s subtree with a fresh
// `<integer>` literal node holding `literal`. Sibling links are preserved.
int Optimizer::replaceVariableReads(TreeNode *parent, const std::string &name, const std::string &literal)
{
    if (!parent) return 0;
    int replaced = 0;
    TreeNode *prev = nullptr;
    for (TreeNode *c = parent->left; c != nullptr;)
    {
        if (c->value == "<identifier>" && c->left && c->left->value == name)
        {
            TreeNode *lit = new TreeNode("<integer>", c->line, c->column);
            lit->left = new TreeNode(literal, c->line, c->column);
            lit->right = c->right; // keep position in the sibling chain
            if (prev == nullptr) parent->left = lit;
            else prev->right = lit;
            replaced++;
            prev = lit;
            c = lit->right;
        }
        else
        {
            replaced += replaceVariableReads(c, name, literal);
            prev = c;
            c = c->right;
        }
    }
    return replaced;
}

// Single-assignment constant propagation over the program body's top-level
// statements. A global qualifies only when ALL of these hold (a sufficient
// condition for the single definition to dominate every use):
//   - it is not referenced by any subprogram (so its whole lifetime is here),
//   - it is written exactly once in the body, by a top-level `v := <integer>`,
//   - no earlier top-level statement references it.
// Then every later read is rewritten to the literal and the assignment removed.
Result<void> Optimizer::propagateConstants(TreeNode *body, const std::unordered_set<std::string> &eligible)
{
    if (!body || !body->left) return Result<void>::Ok();

    TreeNode *prevStmt = nullptr;
    for (TreeNode *stmt = body->left; stmt != nullptr;)
    {
        TreeNode *nextStmt = stmt->right;
        bool removed = false;

        if (stmt->value == "assign" && stmt->left && stmt->left->value == "<identifier>" &&
            stmt->left->left)
        {
            const std::string name = stmt->left->left->value;
            TreeNode *rhs = stmt->left->right;
            // Only names the caller vouched for (confined to this body) may propagate.
            if (eligible.count(name) && rhs && rhs->value == "<integer>" && rhs->left)
            {
                const std::string literal = rhs->left->value;
                if (countVariableWrites(body, name) == 1)
                {
                    bool referencedBefore = false;
                    for (TreeNode *s = body->left; s != stmt; s = s->right)
                        if (subtreeReferences(s, name)) { referencedBefore = true; break; }

                    if (!referencedBefore)
                    {
                        for (TreeNode *s = nextStmt; s != nullptr; s = s->right)
                            replaceVariableReads(s, name, literal);
                        if (prevStmt == nullptr) body->left = nextStmt;
                        else prevStmt->right = nextStmt;
                        instructionRemovedCount++;
                        removed = true;
                    }
                }
            }
        }

        if (!removed) prevStmt = stmt;
        stmt = nextStmt;
    }
    return Result<void>::Ok();
}

// Constant-propagate within each function body, restricted to that function's own
// locals (which are confined to the body by scoping, so the analysis is sound).
Result<void> Optimizer::propagateConstantsInFunctions(TreeNode *subprogs)
{
    if (!subprogs) return Result<void>::Ok();
    for (TreeNode *fcn = subprogs->left; fcn != nullptr; fcn = fcn->right)
    {
        TreeNode *name = fcn->left; // function name identifier
        if (!name) continue;
        // Layout: name, params, returnType, consts, types, dclns, body, endName.
        TreeNode *params = name->right;     if (!params) continue;
        TreeNode *returnType = params->right; if (!returnType) continue;
        TreeNode *consts = returnType->right; if (!consts) continue;
        TreeNode *types = consts->right;    if (!types) continue;
        TreeNode *dclns = types->right;     if (!dclns) continue;
        TreeNode *body = dclns->right;      if (!body) continue;

        std::unordered_set<std::string> locals;
        for (const auto &local : collectDeclaredNames(dclns))
            locals.insert(local);
        if (locals.empty()) continue;

        auto r = propagateConstants(body, locals);
        if (r.isErr()) return r;
    }
    return Result<void>::Ok();
}

// Remove functions that are never called. A function counts as called only if its
// name is referenced OUTSIDE its own subtree (the main body or another function),
// so a recursive function reachable from nowhere is still removed. Iterates to a
// fixpoint because removing one function can make another (only it called) dead.
Result<void> Optimizer::removeUnusedFunctions(TreeNode *subprogs, TreeNode *body)
{
    if (!subprogs) return Result<void>::Ok();
    bool changed = true;
    while (changed)
    {
        changed = false;
        TreeNode *prev = nullptr;
        for (TreeNode *fcn = subprogs->left; fcn != nullptr;)
        {
            TreeNode *next = fcn->right;
            TreeNode *nameId = fcn->left;
            std::string fname = (nameId && nameId->left) ? nameId->left->value : "";

            bool used = !fname.empty() && subtreeReferences(body, fname);
            if (!used && !fname.empty())
            {
                for (TreeNode *other = subprogs->left; other != nullptr; other = other->right)
                {
                    if (other != fcn && subtreeReferences(other, fname)) { used = true; break; }
                }
            }

            if (!used)
            {
                if (prev == nullptr) subprogs->left = next;
                else prev->right = next;
                addWarning("Removed unused function '" + fname + "'.",
                           nameId ? nameId->line : -1, nameId ? nameId->column : -1);
                changed = true;
                fcn = next;
                continue;
            }
            prev = fcn;
            fcn = next;
        }
    }
    return Result<void>::Ok();
}