#pragma once
// Person 1: hash table with chaining (each bucket is our LinkedList).
// Used for: articleID -> Article, domain -> reputation, keyword -> article IDs, ...
#include <string>
#include "LinkedList.h"
using namespace std;

// ---- hash functions ----
inline unsigned long long hashKey(const string& s) {          // djb2
    unsigned long long h = 5381;
    for (size_t i = 0; i < s.size(); ++i)
        h = ((h << 5) + h) + (unsigned char)s[i];
    return h;
}
inline unsigned long long hashKey(int k) {                    // multiplicative
    return (unsigned long long)(unsigned int)k * 2654435761ULL;
}

template <typename K, typename V>
class HashTable {
    struct Entry {
        K key;
        V value;
        Entry(const K& k, const V& v) : key(k), value(v) {}
    };

    LinkedList<Entry>* buckets;
    int capacity;
    int count;

    int indexFor(const K& key) const { return (int)(hashKey(key) % (unsigned long long)capacity); }

    // Grow and re-distribute every entry (keeps chains short).
    void rehash(int newCapacity) {
        LinkedList<Entry>* old = buckets;
        int oldCapacity = capacity;
        buckets = new LinkedList<Entry>[newCapacity];
        capacity = newCapacity;
        for (int i = 0; i < oldCapacity; ++i) {
            typename LinkedList<Entry>::Node* cur = old[i].getHead();
            for (; cur != nullptr; cur = cur->next)
                buckets[indexFor(cur->data.key)].insertBack(cur->data);
        }
        delete[] old;
    }

public:
    explicit HashTable(int initialCapacity = 16)
        : buckets(new LinkedList<Entry>[initialCapacity]), capacity(initialCapacity), count(0) {}

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    ~HashTable() { delete[] buckets; }

    // true  -> new key inserted
    // false -> key already existed, value overwritten
    bool insert(const K& key, const V& value) {
        LinkedList<Entry>& chain = buckets[indexFor(key)];
        Entry* e = chain.findIf([&](const Entry& x) { return x.key == key; });
        if (e != nullptr) {
            e->value = value;
            return false;
        }
        chain.insertBack(Entry(key, value));
        ++count;
        if (count * 4 > capacity * 3) rehash(capacity * 2);   // load factor > 0.75
        return true;
    }

    // Pointer to the stored value, or nullptr if the key is missing.
    V* search(const K& key) {
        Entry* e = buckets[indexFor(key)].findIf([&](const Entry& x) { return x.key == key; });
        return e ? &e->value : nullptr;
    }

    const V* search(const K& key) const {
        const Entry* e = buckets[indexFor(key)].findIf([&](const Entry& x) { return x.key == key; });
        return e ? &e->value : nullptr;
    }

    bool contains(const K& key) const { return search(key) != nullptr; }

    bool remove(const K& key) {
        bool removed = buckets[indexFor(key)].removeIf([&](const Entry& x) { return x.key == key; });
        if (removed) --count;
        return removed;
    }

    int size() const { return count; }
    int bucketCount() const { return capacity; }

    // f(const K& key, V& value) for every entry (order is arbitrary).
    template <typename F>
    void forEach(F f) {
        for (int i = 0; i < capacity; ++i) {
            typename LinkedList<Entry>::Node* cur = buckets[i].getHead();
            for (; cur != nullptr; cur = cur->next) f(cur->data.key, cur->data.value);
        }
    }
};
