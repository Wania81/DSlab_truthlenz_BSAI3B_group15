#pragma once
// Person 3: undirected graph, adjacency list built on our LinkedList.
// Node = article ID. Edge = two articles share >= N keywords (the rule lives in main.cpp).
#include "LinkedList.h"
#include "HashTable.h"
#include <utility>

class Graph {
    struct GNode {
        int id = 0;
        LinkedList<int> adj;     // IDs of neighbouring articles
    };

    GNode* nodes;
    int capacity;
    int n;                       // number of nodes
    int edgeTotal;
    HashTable<int, int> index;   // article ID -> position in 'nodes'

    int indexOf(int id) const {
        const int* p = index.search(id);
        return p ? *p : -1;
    }

    void grow() {
        int newCap = capacity * 2;
        GNode* bigger = new GNode[newCap];
        for (int i = 0; i < n; ++i) bigger[i] = std::move(nodes[i]);
        delete[] nodes;
        nodes = bigger;
        capacity = newCap;
    }

    // Recursive DFS: collects every node reachable from position 'idx'.
    void dfs(int idx, bool* visited, LinkedList<int>& comp) const {
        visited[idx] = true;
        comp.insertBack(nodes[idx].id);
        for (LinkedList<int>::Node* p = nodes[idx].adj.getHead(); p != nullptr; p = p->next) {
            int nb = indexOf(p->data);
            if (nb >= 0 && !visited[nb]) dfs(nb, visited, comp);
        }
    }

public:
    Graph() : nodes(new GNode[16]), capacity(16), n(0), edgeTotal(0) {}
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    ~Graph() { delete[] nodes; }

    bool hasNode(int id) const { return indexOf(id) >= 0; }

    bool addNode(int id) {
        if (hasNode(id)) return false;
        if (n == capacity) grow();
        nodes[n].id = id;
        index.insert(id, n);
        ++n;
        return true;
    }

    // Undirected edge. false if a node is missing, a == b, or the edge already exists.
    bool addEdge(int a, int b) {
        int ia = indexOf(a), ib = indexOf(b);
        if (ia < 0 || ib < 0 || a == b) return false;
        if (nodes[ia].adj.contains(b)) return false;
        nodes[ia].adj.insertBack(b);
        nodes[ib].adj.insertBack(a);
        ++edgeTotal;
        return true;
    }

    // Direct neighbours of an article, or nullptr if the article is not in the graph.
    const LinkedList<int>* neighbors(int id) const {
        int i = indexOf(id);
        return i < 0 ? nullptr : &nodes[i].adj;
    }

    // BFS from startID. 'order' = IDs in visiting order (start first),
    // 'dist'  = hop distance of each ID from the start (same order).
    void bfs(int startId, LinkedList<int>& order, LinkedList<int>& dist) const {
        order.clear();
        dist.clear();
        int s = indexOf(startId);
        if (s < 0) return;

        bool* visited = new bool[n]();
        int* hops = new int[n]();
        LinkedList<int> queue;                       // queue of node positions
        visited[s] = true;
        queue.insertBack(s);

        int cur;
        while (queue.removeFront(cur)) {
            order.insertBack(nodes[cur].id);
            dist.insertBack(hops[cur]);
            for (LinkedList<int>::Node* p = nodes[cur].adj.getHead(); p != nullptr; p = p->next) {
                int nb = indexOf(p->data);
                if (nb >= 0 && !visited[nb]) {
                    visited[nb] = true;
                    hops[nb] = hops[cur] + 1;
                    queue.insertBack(nb);
                }
            }
        }
        delete[] visited;
        delete[] hops;
    }

    // Story clusters = connected components, found with DFS.
    void connectedComponents(LinkedList<LinkedList<int>>& comps) const {
        comps.clear();
        bool* visited = new bool[n]();
        for (int i = 0; i < n; ++i) {
            if (!visited[i]) {
                LinkedList<int> comp;
                dfs(i, visited, comp);
                comps.insertBack(comp);
            }
        }
        delete[] visited;
    }

    int nodeCount() const { return n; }
    int edgeCount() const { return edgeTotal; }
};
