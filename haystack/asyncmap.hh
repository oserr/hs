#pragma once

#include <mutex>
#include <unordered_map>

/**
 * Simple wrapper around an unordered_map to make it thread safe.
 *
 * The full functionality provided by unordered_map is not needed, so AsyncMap
 * only exposes a Get, Put, and Remove operation.
 */
template<typename TKey, typename TValue>
class AsyncMap
{
    using LockGuard = std::lock_guard<std::mutex>;

    mutable std::mutex mtx;
    std::unordered_map<TKey, TValue> hMap;

public:
    AsyncMap() = default;
    AsyncMap(unsigned size);

    bool Get(const TKey &key, TValue &value) const noexcept;
    bool Put(const TKey &key, const TValue &value);
    bool Remove(const TKey &key) noexcept;
};


/**
 * Initializes the map with a given number of buckets.
 * @param size The number of initial buckets in the map.
 */
template<typename TKey, typename TValue>
AsyncMap<TKey, TValue>::AsyncMap(unsigned size)
    : mtx(), hMap(size)
{}


/**
 * Gets a value from the map.
 * @param key The key used to find the value.
 * @param value A reference to the object where the value is copied into.
 * @return True if the key is found, false otherwise.
 */
template<typename TKey, typename TValue>
bool
AsyncMap<TKey, TValue>::Get(const TKey &key, TValue &value) const noexcept
{
    LockGuard lck(mtx);
    auto item = hMap.find(key);
    if (item == hMap.end())
        return false;
    value = item->second;
    return true;
}

/**
 * Puts a value into the map.
 * @param key The associated with the value.
 * @param value The value to be placed in the map.
 * @return True if the value is inserted into the map, false otherwise, i.e.,
 *  the map already contains a value with the same key.
 */
template<typename TKey, typename TValue>
bool
AsyncMap<TKey, TValue>::Put(const TKey &key, const TValue &value)
{
    LockGuard lck(mtx);
    auto result = hMap.emplace(key, value);
    return result.second;
}

/**
 * Removes a value from the map.
 * @param key The associated with the value.
 * @return True if the key is found and the value removed, false otherwise.
 */
template<typename TKey, typename TValue>
bool
AsyncMap<TKey, TValue>::Remove(const TKey &key) noexcept
{
    LockGuard lck(mtx);
    return static_cast<bool>(hMap.erase(key));
}
