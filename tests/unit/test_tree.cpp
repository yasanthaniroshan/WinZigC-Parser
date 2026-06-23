#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "utils/tree.h"

namespace {

// Helper: build a tree with N children of `parent`, each being a leaf labelled
// "child_<i>". The tree owns the children, so deleting parent cleans up.
TreeNode* makeNodeWithChildren(const std::string& label,
                               const std::vector<std::string>& children) {
    auto* parent = new TreeNode(label);
    TreeNode* last = nullptr;
    for (const auto& name : children) {
        auto* node = new TreeNode(name);
        if (last == nullptr) {
            parent->left = node;
        } else {
            last->right = node;
        }
        last = node;
    }
    return parent;
}

}  // namespace

TEST(TreeTest, LeafNodeHasNoChildren) {
    TreeNode leaf("leaf");
    EXPECT_EQ(countChildren(&leaf), 0);
}

TEST(TreeTest, CountChildrenWalksRightSpine) {
    TreeNode* parent = makeNodeWithChildren("root", {"a", "b", "c", "d"});
    EXPECT_EQ(countChildren(parent), 4);
    delete parent;
}

TEST(TreeTest, SingleChildIsCountedOnce) {
    TreeNode* parent = makeNodeWithChildren("root", {"only"});
    EXPECT_EQ(countChildren(parent), 1);
    delete parent;
}

TEST(TreeTest, TreeToStringRendersLeaf) {
    TreeNode leaf("hello");
    EXPECT_EQ(treeToString(&leaf), "hello(0)\n");
}

TEST(TreeTest, TreeToStringIndentsAtDepth) {
    TreeNode leaf("hello");
    EXPECT_EQ(treeToString(&leaf, 2), ". . hello(0)\n");
}

TEST(TreeTest, TreeToStringRendersChildrenWithCounts) {
    TreeNode* parent = makeNodeWithChildren("root", {"a", "b"});
    const std::string out = treeToString(parent);
    EXPECT_EQ(out,
              "root(2)\n"
              ". a(0)\n"
              ". b(0)\n");
    delete parent;
}

TEST(TreeTest, TreeToStringHandlesNestedChildren) {
    auto* root = new TreeNode("root");
    auto* a = new TreeNode("a");
    auto* a1 = new TreeNode("a1");
    auto* a2 = new TreeNode("a2");
    auto* b = new TreeNode("b");
    root->left = a;
    a->left = a1;
    a1->right = a2;
    a->right = b;

    const std::string out = treeToString(root);
    EXPECT_EQ(out,
              "root(2)\n"
              ". a(2)\n"
              ". . a1(0)\n"
              ". . a2(0)\n"
              ". b(0)\n");
    delete root;
}

TEST(TreeTest, TreeToStringOnNullReturnsEmpty) {
    EXPECT_EQ(treeToString(nullptr), "");
}

TEST(TreeTest, PrintTreeWritesToStdoutWithoutCrashing) {
    TreeNode leaf("hi");
    testing::internal::CaptureStdout();
    printTree(&leaf, 0);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "hi(0)\n");
}
