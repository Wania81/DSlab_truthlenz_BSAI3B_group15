#pragma once
// Person 2: min-heap on credibility score (lowest score = most suspicious at the top).
// Ties on score are broken by smaller article ID so the order is deterministic.

struct HeapItem {
    int score;
    int id;
};

class MinHeap {
    HeapItem* data;
    int capacity;
    int count;

    static bool lessThan(const HeapItem& a, const HeapItem& b) {
        if (a.score != b.score) return a.score < b.score;
        return a.id < b.id;
    }

    void grow() {
        int newCap = capacity * 2;
        HeapItem* bigger = new HeapItem[newCap];
        for (int i = 0; i < count; ++i) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        capacity = newCap;
    }

    void heapifyUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (!lessThan(data[i], data[parent])) break;
            HeapItem tmp = data[i]; data[i] = data[parent]; data[parent] = tmp;
            i = parent;
        }
    }

    void heapifyDown(int i) {
        while (true) {
            int l = 2 * i + 1, r = 2 * i + 2, smallest = i;
            if (l < count && lessThan(data[l], data[smallest])) smallest = l;
            if (r < count && lessThan(data[r], data[smallest])) smallest = r;
            if (smallest == i) break;
            HeapItem tmp = data[i]; data[i] = data[smallest]; data[smallest] = tmp;
            i = smallest;
        }
    }

public:
    explicit MinHeap(int initialCapacity = 16)
        : data(new HeapItem[initialCapacity]), capacity(initialCapacity), count(0) {}
    MinHeap(const MinHeap&) = delete;
    MinHeap& operator=(const MinHeap&) = delete;
    ~MinHeap() { delete[] data; }

    void insert(int score, int id) {
        if (count == capacity) grow();
        data[count].score = score;
        data[count].id = id;
        heapifyUp(count);
        ++count;
    }

    bool peek(HeapItem& out) const {
        if (count == 0) return false;
        out = data[0];
        return true;
    }

    bool extractMin(HeapItem& out) {
        if (count == 0) return false;
        out = data[0];
        data[0] = data[count - 1];
        --count;
        if (count > 0) heapifyDown(0);
        return true;
    }

    int size() const { return count; }
    bool empty() const { return count == 0; }
};
