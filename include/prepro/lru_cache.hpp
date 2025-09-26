//
// Created by aowei on 2025 9月 26.
//

#ifndef CC11_PREPRO_LRU_CACHE_HPP
#define CC11_PREPRO_LRU_CACHE_HPP

#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>

// 通用 LRU 缓存模板，考虑到了线程安全
namespace c11::prepro {
template<typename Key, typename Value>
class LRUCache {
public:
    // 构造函数：禁止空容量
    explicit LRUCache(const std::uint64_t max_size) : max_size_(max_size) {
        if (max_size == 0) {
            throw std::invalid_argument("LRU cache max size cannot be zero!");
        }
    }

    // 线程安全存储值
    void put(const Key &key, const Value &value) {
        // 使用独占锁保证线程安全
        std::lock_guard<std::mutex> lock_guard(this->mutex_);
        auto it = this->cache_map_.find(key);
        // 1. 当存储的键已经存在的时候：将数据移动到链表的头部
        if (it != this->cache_map_.end()) {
            // 删除当前位置的键值对，将键值对移动到链表头部
            this->cache_list_.erase(it->second);
            this->cache_list_.push_front({key, value});
            // 重置迭代器当前键对应的链表迭代器的值
            it->second = this->cache_list_.begin();
            return;
        }
        //2. 当缓存满的时候：淘汰尾部（最久未使用的元素）
        if (this->cache_list_.size() >= this->max_size_) {
            // 获取链表尾部的迭代器
            auto last = std::prev(this->cache_list_.end());
            // 删除尾部缓存
            this->cache_map_.erase(last->first);
            this->cache_list_.erase(last);
        }
        // 3. 新增键值对：将键值对插入链表头部
        this->cache_list_.push_front({key, value});
        this->cache_map_[key] = cache_list_.begin();
    }

    // 线程安全取值
    std::optional<Value> get(const Key &key) {
        std::lock_guard<std::mutex> lock_guard(this->mutex_);
        auto it = this->cache_map_.find(key);
        // 当键不存在的时候：返回空
        if (it == this->cache_map_.end()) {
            return std::nullopt;
        }
        // 当键存在的时候：将键移动到头部
        this->cache_list_.splice(this->cache_list_.begin(), this->cache_list_, it->second);
        // map->pair->value
        return it->second->second;
    }

    // 线程安全删除指定键
    void erase(const Key &key) {
        std::lock_guard<std::mutex> lock_guard(this->mutex_);
        auto it = this->cache_map_.find(key);
        // 当找到对应键的时候删除对应数据
        if (it != this->cache_map_.end()) {
            this->cache_list_.erase(it->second);
            this->cache_map_.erase(it);
        }
    }

    // 线程安全清空缓存
    void clear() {
        std::lock_guard<std::mutex> lock_guard(this->mutex_);
        this->cache_map_.clear();
        this->cache_list_.clear();
    }

    // 线程安全获取当前缓存的大小
    std::uint64_t size() const {
        std::lock_guard<std::mutex> lock_guard(this->mutex_);
        return this->cache_list_.size();
    }

private:
    // 缓存的最大容量（不可修改）
    const std::uint64_t max_size_;
    // 链表：头部是最近使用的数据，尾部是最久未使用到的数据（存储键值对）
    std::list<std::pair<Key, Value>> cache_list_;
    // 哈希表：键 -> 链表迭代起（用于快速定位，避免遍历链表）
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
    // 线程安全锁（mutable：const 函数可修改）
    mutable std::mutex mutex_;
};
}

#endif //CC11_PREPRO_LRU_CACHE_HPP
