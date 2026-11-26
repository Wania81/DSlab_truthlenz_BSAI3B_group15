#pragma once
// Person 1: our own singly linked list (template).
// Used for: hash table chaining, triggered indicators (flags), analysis history,
// graph adjacency lists, BFS queue, keyword -> article-ID lists.
#include <iostream>
#include <string>
#include <utility>
using namespace std;

template <typename T>
class LinkedList {
public:
    struct Node {
        T data;
        Node* next;
        explicit Node(const T& d) : data(d), next(nullptr) {}
    };

private:
    Node* head;
    Node* tail;
    int count;

    void copyFrom(const LinkedList& other) {
        for (Node* cur = other.head; cur != nullptr; cur = cur->next)
            insertBack(cur->data);
    }

public:
    LinkedList() : head(nullptr), tail(nullptr), count(0) {}

    // Rule of five: lists get copied (stored inside hash-table entries, etc.)
    LinkedList(const LinkedList& other) : head(nullptr), tail(nullptr), count(0) { copyFrom(other); }

    LinkedList(LinkedList&& other) noexcept
        : head(other.head), tail(other.tail), count(other.count) {
        other.head = other.tail = nullptr;
        other.count = 0;
    }

    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    LinkedList& operator=(LinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            head = other.head;
            tail = other.tail;
            count = other.count;
            other.head = other.tail = nullptr;
            other.count = 0;
        }
        return *this;
    }

    ~LinkedList() { clear(); }

    void insertFront(const T& value) {
        Node* n = new Node(value);
        n->next = head;
        head = n;
        if (tail == nullptr) tail = n;
        ++count;
    }

    void insertBack(const T& value) {
        Node* n = new Node(value);
        if (tail == nullptr) head = tail = n;
        else { tail->next = n; tail = n; }
        ++count;
    }

    // Removes the first node and copies its value into 'out'. false if list is empty.
    bool removeFront(T& out) {
        if (head == nullptr) return false;
        Node* n = head;
        out = n->data;
        head = n->next;
        if (head == nullptr) tail = nullptr;
        delete n;
        --count;
        return true;
    }

    // Removes the first node for which pred(data) is true.
    template <typename Pred>
    bool removeIf(Pred pred) {
        Node* prev = nullptr;
        for (Node* cur = head; cur != nullptr; prev = cur, cur = cur->next) {
            if (pred(cur->data)) {
                if (prev == nullptr) head = cur->next;
                else prev->next = cur->next;
                if (cur == tail) tail = prev;
                delete cur;
                --count;
                return true;
            }
        }
        return false;
    }

    // Removes the first node equal to 'value' (needs operator==).
    bool remove(const T& value) {
        return removeIf([&](const T& x) { return x == value; });
    }

    template <typename Pred>
    T* findIf(Pred pred) {
        for (Node* cur = head; cur != nullptr; cur = cur->next)
            if (pred(cur->data)) return &cur->data;
        return nullptr;
    }

    template <typename Pred>
    const T* findIf(Pred pred) const {
        for (Node* cur = head; cur != nullptr; cur = cur->next)
            if (pred(cur->data)) return &cur->data;
        return nullptr;
    }

    T* find(const T& value) {
        return findIf([&](const T& x) { return x == value; });
    }

    bool contains(const T& value) const {
        return findIf([&](const T& x) { return x == value; }) != nullptr;
    }

    int size() const { return count; }
    bool empty() const { return count == 0; }
    Node* getHead() const { return head; }

    void clear() {
        while (head != nullptr) {
            Node* n = head;
            head = head->next;
            delete n;
        }
        tail = nullptr;
        count = 0;
    }

    // Visit every element: f(const T&)
    template <typename F>
    void forEach(F f) const {
        for (Node* cur = head; cur != nullptr; cur = cur->next) f(cur->data);
    }

    // Prints elements separated by 'sep' (needs operator<< for T).
    void display(ostream& os = cout, const string& sep = ", ") const {
        for (Node* cur = head; cur != nullptr; cur = cur->next) {
            os << cur->data;
            if (cur->next != nullptr) os << sep;
        }
    }
};
