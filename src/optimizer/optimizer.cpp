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
        while (numberOfPasses < MAX_PASSES)
        {
            instructionRemovedCount = 0;
            auto result = constantFoldingPass();
            if (result.isErr())
            {
                return Result<TreeNode *>::Err(OptimizerError(result.error_message.value()));
            }
            LOG_INFO("instructions removed: " + std::to_string(instructionRemovedCount));
            if (instructionRemovedCount == 0)
            {
                break; // No more optimizations possible
            }
            numberOfPasses++;
        }
        return Result<TreeNode *>::Ok(ast);
    }
    // TODO: O1+ passes (constant folding, dead-code elimination) go here.
    return Result<TreeNode *>::Ok(ast);
}

Result<void> Optimizer::constantFoldingPass()
{
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
    LOG_INFO("Removed " + std::to_string(instructionRemovedCount) + " minus nodes.");
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
    LOG_INFO("Removed " + std::to_string(instructionRemovedCount) + " constant addition nodes.");
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
    LOG_INFO("Removed " + std::to_string(instructionRemovedCount) + " constant subtraction nodes.");
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
    LOG_INFO("Removed " + std::to_string(instructionRemovedCount) + " constant multiplication nodes.");
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
    LOG_INFO("Removed " + std::to_string(instructionRemovedCount) + " constant division nodes.");
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

Result<void> Optimizer::removeConstantBitwiseAnd(TreeNode *node)
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
        int sum = leftValue & rightValue;
        TreeNode *andNode = new TreeNode("<integer>", node->line, node->column);
        andNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(andNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeConstantBitwiseOr(TreeNode *node)
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
        int sum = leftValue | rightValue;
        TreeNode *orNode = new TreeNode("<integer>", node->line, node->column);
        orNode->left = new TreeNode(std::to_string(sum), node->line, node->column);
        treeTraveler.swap(orNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}


