#include "utils/tree.h"
#include <iostream>
#include <sstream>
#include <utility>

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

TreeNode* TreeTraveler::step() {
    if (!started) {
        started = true;
        if (current) {
            if (searchMethod == SearchMethod::DEPTH_FIRST) {
                dfsStack.push_back({current, nullptr});
            } else {
                bfsQueue.push({current, nullptr});
            }
        }
    }

    if (searchMethod == SearchMethod::DEPTH_FIRST) {
        while (!dfsStack.empty()) {
            auto [node, slot] = dfsStack.back();
            dfsStack.pop_back();
            // Push right before left so left is the next one popped, preserving left-to-right order.
            // Skip the root's own right sibling: it lies outside the subtree we're scoped to. (Children, reached via left and their own right links, are still traversed.)
            if (node != root && node->right) dfsStack.push_back({node->right, &node->right});
            if (node->left) dfsStack.push_back({node->left, &node->left});
            current = node;
            if (node->value == nodeValue) {
                pointer = node;
                pointerSlot = slot;
                return pointer;
            }
        }
    } else { // BREADTH_FIRST
        while (!bfsQueue.empty()) {
            auto [node, slot] = bfsQueue.front();
            bfsQueue.pop();
            if (node->left) bfsQueue.push({node->left, &node->left});
            // Skip the root's own right sibling: it lies outside the subtree we're scoped to.
            if (node != root && node->right) bfsQueue.push({node->right, &node->right});
            current = node;
            if (node->value == nodeValue) {
                pointer = node;
                pointerSlot = slot;
                return pointer;
            }
        }
    }

    return nullptr; // Frontier exhausted: no (more) matching node.
}

bool TreeTraveler::swap(TreeNode* newNode) {
    if (!pointer || !newNode || !pointerSlot) return false;

    newNode->right = pointer->right; // keep pointer's old place in its sibling chain
    *pointerSlot = newNode;
    return true;
}