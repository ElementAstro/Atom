/*
 * atom/containers/intrusive.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: Boost Intrusive Containers

**************************************************/

#pragma once

#include "../macro.hpp"

// 只有在定义了ATOM_USE_BOOST_INTRUSIVE宏且Boost侵入式容器库可用时才启用
#if defined(ATOM_HAS_BOOST_INTRUSIVE)

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/functional/hash.hpp>

namespace atom {
namespace containers {
namespace intrusive {

// 定义常用链表钩子
using list_base_hook = boost::intrusive::list_base_hook<>;
using set_base_hook = boost::intrusive::set_base_hook<>;
using unordered_set_base_hook = boost::intrusive::unordered_set_base_hook<>;
using slist_base_hook = boost::intrusive::slist_base_hook<>;

/**
 * @brief 侵入式链表
 * 
 * 侵入式链表要求元素类型内包含钩子（hook），避免了额外的内存分配。
 * 非常适合管理大量对象，减少内存碎片和提高缓存性能。
 * 
 * 使用示例:
 * class MyClass : public atom::containers::intrusive::list_base_hook {
 *   // 类成员和方法
 * };
 * 
 * atom::containers::intrusive::list<MyClass> my_list;
 * 
 * @tparam T 必须继承自list_base_hook的元素类型
 */
template <typename T>
using list = boost::intrusive::list<T>;

/**
 * @brief 侵入式单向链表
 * 
 * 比双向链表更轻量，但只支持单向遍历
 * 
 * @tparam T 必须继承自slist_base_hook的元素类型
 */
template <typename T>
using slist = boost::intrusive::slist<T>;

/**
 * @brief 侵入式有序集合
 * 
 * 元素按键排序，提供快速查找，同时避免了内存分配开销
 * 
 * @tparam T 必须继承自set_base_hook的元素类型
 * @tparam Compare 比较元素的函数对象类型
 */
template <typename T, typename Compare = std::less<T>>
using set = boost::intrusive::set<T, boost::intrusive::compare<Compare>>;

/**
 * @brief 侵入式无序集合
 * 
 * 通过哈希实现快速查找，避免了标准无序容器的节点分配开销
 * 
 * @tparam T 必须继承自unordered_set_base_hook的元素类型
 * @tparam Hash 哈希函数对象类型
 * @tparam Equal 判断元素相等的函数对象类型
 */
template <typename T, 
          typename Hash = boost::hash<T>,
          typename Equal = std::equal_to<T>>
class unordered_set {
private:
    // 哈希表桶的基本配置
    static constexpr std::size_t NumBuckets = 128;
    using bucket_type = boost::intrusive::unordered_set<T>::bucket_type;
    bucket_type buckets_[NumBuckets];
    
    using unordered_set_type = boost::intrusive::unordered_set<
        T,
        boost::intrusive::hash<Hash>,
        boost::intrusive::equal<Equal>,
        boost::intrusive::constant_time_size<true>
    >;
    
    unordered_set_type set_;
    
public:
    using iterator = typename unordered_set_type::iterator;
    using const_iterator = typename unordered_set_type::const_iterator;
    
    unordered_set() : set_(boost::intrusive::bucket_traits(buckets_, NumBuckets)) {}
    
    /**
     * @brief 插入元素到无序集合
     * 
     * @param value 要插入的元素
     * @return std::pair<iterator, bool> 包含指向插入元素的迭代器和是否成功插入的标志
     */
    std::pair<iterator, bool> insert(T& value) {
        return set_.insert(value);
    }
    
    /**
     * @brief 从无序集合中移除元素
     * 
     * @param value 要移除的元素
     * @return bool 如果元素被移除则返回true
     */
    bool remove(T& value) {
        return set_.erase(value) > 0;
    }
    
    /**
     * @brief 查找元素
     * 
     * @param value 要查找的元素
     * @return iterator 指向找到的元素，如果未找到则返回end()
     */
    iterator find(const T& value) {
        return set_.find(value);
    }
    
    /**
     * @brief 返回起始迭代器
     */
    iterator begin() {
        return set_.begin();
    }
    
    /**
     * @brief 返回终止迭代器
     */
    iterator end() {
        return set_.end();
    }
    
    /**
     * @brief 检查容器是否为空
     */
    bool empty() const {
        return set_.empty();
    }
    
    /**
     * @brief 返回容器中元素的数量
     */
    std::size_t size() const {
        return set_.size();
    }
    
    /**
     * @brief 清空容器
     */
    void clear() {
        set_.clear();
    }
};

/**
 * @brief 提供可链接类型的助手基类
 * 
 * 这个类简化了创建支持多种侵入式容器的对象。
 * 如果需要一个对象同时可以放入list、set和unordered_set，
 * 可以继承这个类。
 */
class intrusive_base :
    public list_base_hook,
    public set_base_hook,
    public unordered_set_base_hook,
    public slist_base_hook
{
protected:
    // 保护构造函数防止直接实例化
    intrusive_base() = default;
    
    // 允许派生类销毁
    virtual ~intrusive_base() = default;
    
    // 禁止复制
    intrusive_base(const intrusive_base&) = delete;
    intrusive_base& operator=(const intrusive_base&) = delete;
    
    // 允许移动
    intrusive_base(intrusive_base&&) = default;
    intrusive_base& operator=(intrusive_base&&) = default;
};

} // namespace intrusive
} // namespace containers
} // namespace atom

#endif // defined(ATOM_HAS_BOOST_INTRUSIVE)