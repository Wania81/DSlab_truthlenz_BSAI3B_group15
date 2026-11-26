#pragma once
// Person 2: BST keyed by credibility score.
// Several articles can share a score, so every node keeps a LinkedList of article IDs.
#include "LinkedList.h"
#include <utility>

class BST {
    struct Node {
        int key;                 // credibility score
        LinkedList<int> ids;     // article IDs having exactly this score
        Node* left;
        Node* right;
        explicit Node(int k) : key(k), left(nullptr), right(nullptr) {}
    };

    Node* root;
    int total;                   // number of article IDs stored

    static Node* insertRec(Node* n, int score, int id) {
        if (n == nullptr) {
            Node* fresh = new Node(score);
            fresh->ids.insertBack(id);
            return fresh;
        }
        if (score < n->key) n->left = insertRec(n->left, score, id);
        else if (score > n->key) n->right = insertRec(n->right, score, id);
        else n->ids.insertBack(id);                 // duplicate score
        return n;
    }

    static Node* removeKey(Node* n, int key) {
        if (n == nullptr) return nullptr;
        if (key < n->key) n->left = removeKey(n->left, key);
        else if (key > n->key) n->right = removeKey(n->right, key);
        else {
            if (n->left == nullptr) { Node* r = n->right; delete n; return r; }
            if (n->right == nullptr) { Node* l = n->left; delete n; return l; }
            // two children: take over the in-order successor's data, then delete the successor
            Node* s = n->right;
            while (s->left != nullptr) s = s->left;
            n->key = s->key;
            n->ids = std::move(s->ids);
            n->right = removeKey(n->right, s->key);
        }
        return n;
    }

    static Node* findNode(Node* n, int key) {
        while (n != nullptr && n->key != key)
            n = (key < n->key) ? n->left : n->right;
        return n;
    }

    static void inorderRec(Node* n, LinkedList<int>& out) {
        if (n == nullptr) return;
        inorderRec(n->left, out);
        n->ids.forEach([&](int id) { out.insertBack(id); });
        inorderRec(n->right, out);
    }

    static void rangeRec(Node* n, int low, int high, LinkedList<int>& out) {
        if (n == nullptr) return;
        if (low < n->key) rangeRec(n->left, low, high, out);          // left side may still hold matches
        if (low <= n->key && n->key <= high)
            n->ids.forEach([&](int id) { out.insertBack(id); });
        if (high > n->key) rangeRec(n->right, low, high, out);        // right side may still hold matches
    }

    static void destroy(Node* n) {
        if (n == nullptr) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

public:
    BST() : root(nullptr), total(0) {}
    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;
    ~BST() { destroy(root); }

    void insert(int score, int id) {
        root = insertRec(root, score, id);
        ++total;
    }

    // Removes one article ID stored under 'score'. false if not found.
    bool remove(int score, int id) {
        Node* n = findNode(root, score);
        if (n == nullptr) return false;
        if (!n->ids.remove(id)) return false;
        --total;
        if (n->ids.empty()) root = removeKey(root, score);
        return true;
    }

    // All IDs ordered by score, lowest score first.
    void inorder(LinkedList<int>& out) const {
        out.clear();
        inorderRec(root, out);
    }

    // All IDs with low <= score <= high, in ascending score order.
    void rangeSearch(int low, int high, LinkedList<int>& out) const {
        out.clear();
        if (low > high) return;
        rangeRec(root, low, high, out);
    }

    bool findMin(int& score) const {
        if (root == nullptr) return false;
        Node* n = root;
        while (n->left != nullptr) n = n->left;
        score = n->key;
        return true;
    }

    bool findMax(int& score) const {
        if (root == nullptr) return false;
        Node* n = root;
        while (n->right != nullptr) n = n->right;
        score = n->key;
        return true;
    }

    int size() const { return total; }
    bool empty() const { return total == 0; }
};
