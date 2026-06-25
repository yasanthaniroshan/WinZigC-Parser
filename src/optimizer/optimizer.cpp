#include "optimizer/optimizer.h"
#include <iostream>

Result<TreeNode *> Optimizer::optimize()
{
    if (optimizationLevel == "O0")
    {
        return Result<TreeNode *>::Ok(ast);
    }
    if (optimizationLevel == "O1")
    {
        int numberOfPasses = 0;
        TreeNode *name = ast->left;       // First child is program name
        TreeNode *consts = name->right;    // First sibling is const declarations
        TreeNode *types = consts->right;   // Next sibling is type declarations
        TreeNode *dclns = types->right;    // Next sibling is variable and function declarations
        TreeNode *subprogs = dclns->right; // Next sibling is subprogram declarations
        TreeNode *body = subprogs->right; // Next sibling is the body of the program
        auto resultUnusedVars = removeUnusedVariables(dclns, subprogs, body);
        if (resultUnusedVars.isErr())
        {
            return Result<TreeNode *>::Err(OptimizerError(resultUnusedVars.error_message.value()));
        }
        auto resultUnusedLocals = removeUnusedLocalVariables(subprogs);
        if (resultUnusedLocals.isErr())
        {
            return Result<TreeNode *>::Err(OptimizerError(resultUnusedLocals.error_message.value()));
        }
        while (numberOfPasses < MAX_PASSES)
        {
            instructionRemovedCount = 0;
            auto result = constantFoldingPass();
            if (result.isErr())
            {
                return Result<TreeNode *>::Err(OptimizerError(result.error_message.value()));
            }
            LOG_DEBUG("instructions removed: " + std::to_string(instructionRemovedCount));
            if (instructionRemovedCount == 0)
            {
                break; // No more optimizations possible
            }
            numberOfPasses++;
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
    // TODO: O1+ passes (constant folding, dead-code elimination) go here.
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
