#pragma once

#include "../pch.h"
#include <any>
#include <unordered_map>
#include <unordered_set>

namespace stdext
{
    template <class K, class V,
        class Hash = std::hash<K>,
        class Eq = std::equal_to<K>,
        class Alloc = std::allocator<std::pair<const K, V>>>
    using map = std::unordered_map<K, V, Hash, Eq, Alloc>;

    template <class T,
        class Hash = std::hash<T>,
        class Eq = std::equal_to<T>,
        class Alloc = std::allocator<T>>
    using set = std::unordered_set<T, Hash, Eq, Alloc>;

    template<typename T>
    concept OnlyEnum = std::is_enum_v<T>;

    template<OnlyEnum Key>
    class dynamic_storage
    {
    public:
        template<typename T>
        void set(const Key& key, const T& value) { m_data[key] = value; }

        bool remove(const Key& k) { return m_data.erase(k) > 0; }

        template<typename T> T get(const Key& k, const T defaultValue = T()) const
        {
            auto it = m_data.find(k);
            if (it == m_data.end()) {
                return defaultValue;
            }

            try {
                return std::any_cast<T>(it->second);
            } catch (const std::exception&) {
                return defaultValue;
            }
        }

        bool has(const Key& k) const { return m_data.contains(k); }

        size_t size() const { return m_data.size(); }

        void clear() { m_data.clear(); }

    private:
        map<Key, std::any> m_data;
    };
}
