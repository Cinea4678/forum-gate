//
// Created by cinea on 24-2-23.
//

#ifndef LRULIST_H
#define LRULIST_H
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <algorithm>

template <typename Key, typename Value, typename Lock = std::mutex>
class lru_list : std::enable_shared_from_this<lru_list<Key, Value, Lock>>
{
    typedef std::list<std::pair<Key, Value>> List;
    typedef std::unordered_map<Key, typename List::iterator> Map;

    List list_;
    Map map_;
    Lock lock_;

    size_t max_capacity_;

public:
    explicit lru_list(const size_t max_capacity): max_capacity_(max_capacity)
    {
    }

    [[nodiscard]] std::optional<std::pair<Key, Value>> insert(const Key& key, Value&& new_item);
    bool remove(const Key& key);
};


template <typename Key, typename Value, typename Lock>
std::optional<std::pair<Key, Value>> lru_list<Key, Value, Lock>::insert(const Key& key, Value&& new_item)
{
    const std::lock_guard<Lock> guard(lock_);
    std::optional<std::pair<Key, Value>> popped;

    if (list_.size() >= max_capacity_)
    {
        auto le_it = std::prev(list_.end());
        auto le = std::move(*le_it);
        list_.erase(le_it);
        popped = std::optional<std::pair<Key, Value>>(std::move(le));
    }

    list_.emplace_front(key, std::move(new_item));
    auto new_it = list_.begin();

    if (popped.has_value())
    {
        map_.erase(map_.find(popped.value().first));
    }

    map_.insert_or_assign(key, std::move(new_it));

    return popped;
}

template <typename Key, typename Value, typename Lock>
bool lru_list<Key, Value, Lock>::remove(const Key& key)
{
    auto it = map_.find(key);
    if (it == map_.end())
    {
        return false;
    }

    assert(it->second->first == key);

    list_.erase(it->second);
    map_.erase(it);

    return true;
}


#endif //LRULIST_H
