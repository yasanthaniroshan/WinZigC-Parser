#ifndef TREE_H
#define TREE_H

#include <string>
#include <vector>
#include <queue>
#include <utility>


enum class TravellingMethod {
    PRE_ORDER,
    IN_ORDER,
    POST_ORDER
};

enum class TreeTraversalDirection {
    LEFT,
    RIGHT
};

enum class SearchMethod {
    DEPTH_FIRST,
    BREADTH_FIRST
};

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

class TreeTraveler {
    private:
        TreeNode* current;
        std::string nodeValue;
        TreeNode* pointer = nullptr;
        TreeNode** pointerSlot = nullptr; // Address of the left/right field that points at `pointer`; null if `pointer` is the root.
        SearchMethod searchMethod = SearchMethod::DEPTH_FIRST;
        // Frontier entries are (node, address of the field that points to it), so a later swap() can
        // relink that exact field. Persists across step() calls so the search can resume.
        std::vector<std::pair<TreeNode*, TreeNode**>> dfsStack;
        std::queue<std::pair<TreeNode*, TreeNode**>> bfsQueue;
        bool started = false; // Whether the frontier has been seeded with the root yet.
public:
    explicit TreeTraveler(TreeNode* root) : current(root) {}
    void setNodeValue(const std::string& value) { nodeValue = value; }
    void setSearchMethod(SearchMethod method) { searchMethod = method; }
    TreeNode* step();
    bool swap(TreeNode* newNode);
};

#endif
