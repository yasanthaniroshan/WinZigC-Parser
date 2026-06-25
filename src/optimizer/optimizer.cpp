#include "optimizer/optimizer.h"
#include <iostream>
#include <unordered_set>

Result<TreeNode *> Optimizer::optimize()
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
            auto folded = constantFoldingPass();
            if (folded.isErr())
                return Result<TreeNode *>::Err(OptimizerError(folded.error_message.value()));
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

Result<void> Optimizer::constantFoldingPass()
{
    treeTraveler.setSubtreeRoot(ast);
    treeTraveler.reset();
    treeTraveler.setNodeValue("-");
    while (TreeNode *node = treeTraveler.step())
    {
        auto result = removeMinusNode(node);
        if (result.isErr())
        {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    LOG_DEBUG("Removed " + std::to_string(instructionRemovedCount) + " minus nodes.");
    treeTraveler.reset();
    treeTraveler.setNodeValue("+");
    while (TreeNode *node = treeTraveler.step())
    {
        auto result = removeConstantAddition(node);
        if (result.isErr())
        {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    LOG_DEBUG("Removed " + std::to_string(instructionRemovedCount) + " constant addition nodes.");
    treeTraveler.reset();
    treeTraveler.setNodeValue("-");
    while (TreeNode *node = treeTraveler.step())
    {
        auto result = removeConstantSubtraction(node);
        if (result.isErr())
        {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    LOG_DEBUG("Removed " + std::to_string(instructionRemovedCount) + " constant subtraction nodes.");
    treeTraveler.reset();
    treeTraveler.setNodeValue("*");
    while (TreeNode *node = treeTraveler.step())
    {
        auto result = removeConstantMultiplication(node);
        if (result.isErr())
        {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    LOG_DEBUG("Removed " + std::to_string(instructionRemovedCount) + " constant multiplication nodes.");
    treeTraveler.reset();
    treeTraveler.setNodeValue("/");
    while (TreeNode *node = treeTraveler.step())
    {
        auto result = removeConstantDivision(node);
        if (result.isErr())
        {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    LOG_DEBUG("Removed " + std::to_string(instructionRemovedCount) + " constant division nodes.");
    return Result<void>::Ok();
}

Result<void> Optimizer::removeMinusNode(TreeNode *node)
{
    if (!node->left)
        return Result<void>::Ok();
    if (node->left->value == "<integer>" && node->left->right == nullptr)
    { // only fold if the minus node has a single integer child
        TreeNode *valueNode = node->left->left;
        valueNode->value = std::to_string(-std::stoi(valueNode->value));
        TreeNode *minusNode = new TreeNode("<integer>", node->left->line, node->left->column);
        minusNode->left = valueNode;
        treeTraveler.swap(minusNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantAddition(TreeNode *node)
{
    TreeNode *lhs = node->left;
    if (!lhs)
        return Result<void>::Ok();
    TreeNode *rhs = lhs->right;
    if (!rhs)
        return Result<void>::Ok();
    if (lhs->value == "<integer>" && rhs->value == "<integer>")
    {
        int leftValue = std::stoi(lhs->left->value);
        int rightValue = std::stoi(rhs->left->value);
        int sum = leftValue + rightValue;
        TreeNode *sumNode = new TreeNode("<integer>", node->line, node->column);
        sumNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(sumNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantSubtraction(TreeNode *node)
{
    TreeNode *lhs = node->left;
    if (!lhs)
        return Result<void>::Ok();
    TreeNode *rhs = lhs->right;
    if (!rhs)
        return Result<void>::Ok();
    if (lhs->value == "<integer>" && rhs->value == "<integer>")
    {
        int leftValue = std::stoi(lhs->left->value);
        int rightValue = std::stoi(rhs->left->value);
        int sum = leftValue - rightValue;
        TreeNode *subNode = new TreeNode("<integer>", node->line, node->column);
        subNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(subNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantMultiplication(TreeNode *node)
{
    TreeNode *lhs = node->left;
    if (!lhs)
        return Result<void>::Ok();
    TreeNode *rhs = lhs->right;
    if (!rhs)
        return Result<void>::Ok();
    if (lhs->value == "<integer>" && rhs->value == "<integer>")
    {
        int leftValue = std::stoi(lhs->left->value);
        int rightValue = std::stoi(rhs->left->value);
        int sum = leftValue * rightValue;
        TreeNode *mulNode = new TreeNode("<integer>", node->line, node->column);
        mulNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(mulNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantDivision(TreeNode *node)
{
    TreeNode *lhs = node->left;
    if (!lhs)
        return Result<void>::Ok();
    TreeNode *rhs = lhs->right;
    if (!rhs)
        return Result<void>::Ok();
    if (lhs->value == "<integer>" && rhs->value == "<integer>")
    {
        int leftValue = std::stoi(lhs->left->value);
        int rightValue = std::stoi(rhs->left->value);
        int sum = leftValue / rightValue;
        TreeNode *divNode = new TreeNode("<integer>", node->line, node->column);
        divNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(divNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantModulus(TreeNode *node)
{
    TreeNode *lhs = node->left;
    if (!lhs)
        return Result<void>::Ok();
    TreeNode *rhs = lhs->right;
    if (!rhs)
        return Result<void>::Ok();
    if (lhs->value == "<integer>" && rhs->value == "<integer>")
    {
        int leftValue = std::stoi(lhs->left->value);
        int rightValue = std::stoi(rhs->left->value);
        int sum = leftValue % rightValue;
        TreeNode *modNode = new TreeNode("<integer>", node->line, node->column);
        modNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(modNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
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

// Count how many times `name` is WRITTEN within `node`'s subtree: assignment
// targets, swap operands, and read() targets. Over-counting is safe (it only
// makes propagation more conservative); under-counting would be unsound.
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