#include "utils/tree.h"
#include <iostream>

int countChildren(TreeNode* node) {
    int n = 0;
    for (TreeNode* c = node->left; c != nullptr; c = c->right)
        ++n;
    return n;
}

void printTree(TreeNode* node, int depth) {
    if (!node) return;

    // indent: depth dots + space (match winzig_00.tree)
    for (int i = 0; i < depth; ++i)
        std::cout << ". ";
    std::cout << node->value << "(" << countChildren(node) << ")\n";

    // preorder over children (sibling list)
    for (TreeNode* child = node->left; child != nullptr; child = child->right)
        printTree(child, depth + 1);
}