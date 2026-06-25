#include "optimizer/optimizer.h"
#include <iostream>

Result<TreeNode*> Optimizer::optimize() {
    if (optimizationLevel == "O0") {
        return Result<TreeNode*>::Ok(ast);
    }
    if (optimizationLevel == "O1") {
        auto result = constantFoldingPass();
        if (result.isErr()) {
            return Result<TreeNode*>::Err(OptimizerError(result.error_message.value()));
        }
        LOG_INFO("instructions removed: " + std::to_string(instructionRemovedCount));
        return Result<TreeNode*>::Ok(ast);
    }
    // TODO: O1+ passes (constant folding, dead-code elimination) go here.
    return Result<TreeNode*>::Ok(ast);
}


Result<void> Optimizer::constantFoldingPass() {
    treeTraveler.setNodeValue("-");
    while (TreeNode* node = treeTraveler.step()) {
        auto result = removeMinusNode(node);
        if (result.isErr()) {
            return Result<void>::Err(OptimizerError(result.error_message.value()));
        }
    }
    return Result<void>::Ok();
}

Result<void> Optimizer::removeMinusNode(TreeNode* node) {
    if(!node->left) return Result<void>::Ok();
    if (node->left->value == "<integer>" && node->left->right == nullptr) { // only fold if the minus node has a single integer child
        TreeNode* valueNode = node->left->left;
        valueNode->value = std::to_string(-std::stoi(valueNode->value));
        TreeNode* minusNode = new TreeNode("<integer>", node->left->line, node->left->column);
        minusNode->left = valueNode;
        treeTraveler.swap(minusNode);
        instructionRemovedCount++;
        return Result<void>::Ok();
    }
    return Result<void>::Ok();
}


