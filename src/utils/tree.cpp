#include "utils/tree.h"
#include <iostream>
#include <sstream>

int countChildren(TreeNode* node) {
    int n = 0;
    for (TreeNode* c = node->left; c != nullptr; c = c->right)
        ++n;
    return n;
}

static void appendTreeLines(TreeNode* node, int depth, std::ostream& out) {
    if (!node) return;

    for (int i = 0; i < depth; ++i)
        out << ". ";
    out << node->value << "(" << countChildren(node) << ")\n";

    for (TreeNode* child = node->left; child != nullptr; child = child->right)
        appendTreeLines(child, depth + 1, out);
}

void printTree(TreeNode* node, int depth) {
    appendTreeLines(node, depth, std::cout);
}

std::string treeToString(TreeNode* node, int depth) {
    std::ostringstream out;
    appendTreeLines(node, depth, out);
    return out.str();
}