//
// Created by 17632 on 2023/8/1.
// 基于leecode里面的LRU写的LRU缓存
//  get put时间复杂度都是O1

#ifndef XD_LRU_XD_LRU_H

#include <unordered_set>
#include <unordered_map>

//双向链表
template<typename KeyType, typename ValueType>
struct DLinkedNode {
    KeyType key;
    ValueType value;
    DLinkedNode *prev;
    DLinkedNode *next;

    DLinkedNode() : key(), value(), prev(nullptr), next(nullptr) {}
    DLinkedNode(KeyType _key, ValueType _value) : key(_key), value(_value), prev(nullptr), next(nullptr) {}
};

template<typename KeyType, typename ValueType>
class LRUCache {
public:
    typedef DLinkedNode<KeyType, ValueType> DLink;

private:
    std::unordered_map<KeyType, DLink *> cache;
    DLink *head;
    DLink *tail;
    int size;
    int capacity;

public:
    LRUCache(int _capacity) : capacity(_capacity), size(0) {
        // 使用伪头部和伪尾部节点
        head = new DLink();
        tail = new DLink();
        head->next = tail;
        tail->prev = head;
    }

    ValueType get(KeyType key) {
        if (!cache.count(key)) {
            throw "key not found";
        }
        // 如果 key 存在，先通过哈希表定位，再移到头部
        DLink *node = cache[key];
        moveToHead(node);
        return node->value;
    }

    void put(KeyType key, ValueType value) {
        if (!cache.count(key)) {
            // 如果 key 不存在，创建一个新的节点
            auto *node = new DLink(key, value);
            // 添加进哈希表
            cache[key] = node;
            // 添加至双向链表的头部
            addToHead(node);
            ++size;
            if (size > capacity) {
                // 如果超出容量，删除双向链表的尾部节点
                DLink *removed = removeTail();
                // 删除哈希表中对应的项
                cache.erase(removed->key);
                // 防止内存泄漏
                delete removed;
                --size;
            }
        } else {
            // 如果 key 存在，先通过哈希表定位，再修改 value，并移到头部
            DLink *node = cache[key];
            node->value = value;
            moveToHead(node);
        }
    }

    void addToHead(DLink *node) {
        node->prev = head;
        node->next = head->next;
        head->next->prev = node;
        head->next = node;
    }

    void removeNode(DLink *node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void moveToHead(DLink *node) {
        removeNode(node);
        addToHead(node);
    }

    DLink *removeTail() {
        DLink *node = tail->prev;
        removeNode(node);
        return node;
    }
};


#define XD_LRU_XD_LRU_H

#endif //XD_LRU_XD_LRU_H
