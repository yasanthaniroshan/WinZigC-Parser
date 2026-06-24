#ifndef TREE_H
#define TREE_H

#include <string>

struct TreeNode {
    std::string value;
    TreeNode* left;
    TreeNode* right;
    int line = -1; // Optional: store line number for error reporting
    int column = -1; // Optional: store column number for error reporting

    explicit TreeNode(std::string value, TreeNode* left = nullptr, TreeNode* right = nullptr)
        : value(std::move(value)), left(left), right(right) {}

    TreeNode(std::string value, int line, int column, TreeNode* left = nullptr, TreeNode* right = nullptr)
        : value(std::move(value)), left(left), right(right), line(line), column(column) {}

    ~TreeNode() {
        delete left;
        delete right;
    }

    TreeNode(const TreeNode&) = delete;
    TreeNode& operator=(const TreeNode&) = delete;
};

int countChildren(TreeNode* node);
void printTree(TreeNode* node, int depth);
std::string treeToString(TreeNode* node, int depth = 0);

#endif
