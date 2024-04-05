///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 General Motors GTO LLC
//
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
// SPDX-FileType: SOURCE
// SPDX-FileCopyrightText: 2024 General Motors GTO LLC
// SPDX-License-Identifier: Apache-2.0
///////////////////////////////////////////////////////////////////////////////
 
#ifndef UP_CPP_SAFE_MAP_H
#define UP_CPP_SAFE_MAP_H

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace uprotocol::utils {

// Forward declaration for following using statements. See documentation below
template<typename MapT>
class BaseSafeMap;

///////////////////////////////////////////////////////////////////////////////
/// @name Common map type wrappers
/// Templated type definitions of wrappers for common map types.
/// @{

/// @brief Thread-safe wrapper for std::map
template<class Key, class T,
    class Compare = typename std::map<Key, T>::key_compare,
    class Allocator = typename std::map<Key, T, Compare>::allocator_type>
using SafeMap = BaseSafeMap<std::map<Key, T, Compare, Allocator>>;

/// @brief Thread-safe wrapper for std::unordered_map
template<class Key, class T,
    class Hash = typename std::unordered_map<Key, T>::hasher,
    class KeyEqual = typename std::unordered_map<Key, T, Hash>::key_equal,
    class Allocator = typename std::unordered_map<Key, T, Hash, KeyEqual>::allocator_type>
using SafeUnorderedMap = BaseSafeMap<std::unordered_map<Key, T, Hash, KeyEqual, Allocator>>;
/// @}
///////////////////////////////////////////////////////////////////////////////

namespace detail {
/// @remarks Inherits publicly from MapT
template<typename MapT>
struct CopyMoveCtorLocker;
} // namespace detail

///////////////////////////////////////////////////////////////////////////////
/// @brief Wraps std::map in a std::shared_mutex for thread-safe access
///
/// By using std::shared_mutex, we can avoid serialization of const access to
/// the map. While there is still _some_ performance hit from adding the lock,
/// this mitigates the worst of it.
///
/// Since locks must be held during access, a new transact() interface has been
/// added. This allows for bulk operations, provided in the form of a callable,
/// to be executed while the lock is held.
///
/// Aside from the locking and transactions, all interfaces are passthroughs
/// to the underlying std::map class.
///
/// @tparam MapT Type of map container to be wrapped. Could be std::map or
///              std::unordered_map, for example.
///
/// @note Inherits from MapT via detail::CopyMoveCtorLocker.
///
/// @remarks We use protected inheritance from std::map so that any attempt to
///          use an interface that has not been protected by a mutex will fail
///          to compile with an error that the method is protected.
///
/// @remarks On the subject of iterators and transactions:<br>
///          This wrapper *does not* allow direct access to interfaces that
///          take or return iterators. This is because iterators already can
///          be easily invalidated by many operators, so adding concurrent
///          access makes it nearly impossible to use them. While locking could
///          be added to the iterators themselves, that would introduce new
///          risks (e.g. locks accidentally being held because an iterator
///          was held).
///          <br>
///          The solution is to provide transact() methods that allow for bulk
///          actions while holding the lock. For operations where iterators
///          or atomic operations need to occur on map data, a callable
///          (function pointer, lambda, etc) can be passed to transact(). The
///          lock will be held while the callable is running.
template<typename MapT>
class BaseSafeMap : public detail::CopyMoveCtorLocker<MapT> {
    /// @brief Shared mutex (aka read-write lock) used to protect concurrent
    /// access to the map.
    mutable std::shared_mutex map_lock_{};

    /// @brief Shorthand for the copy/move locking base class this class has
    ///        inherited from.
    using CMCLock = detail::CopyMoveCtorLocker<MapT>;

public:
    /// @brief Public redeclaration of the MapT template parameter so it can
    ///        be used when needed (e.g. in writing lambdas for transact())
    using UnsafeMapT = MapT;

    ///////////////////////////////////////////////////////////////////////////
    /// @name Bulk transactions
    /// Collection of types and interfaces related to executing bulk operations
    /// while the lock is held.
    /// @{

    /// @brief Type definition for a map-modifying callable returning an RT
    template<typename RT>
    using ModifyTxn = std::function<RT(MapT&)>;

    /// @brief Executes a modifying transaction while an exclusive lock is held
    ///
    /// @param f Map-modifying callable to execute
    /// @tparam RT Return value type for f
    ///
    /// @returns Value of RT returned from calling f
    ///
    /// @remarks Template parameter must be specified when passing anonymous
    ///          lambdas of unclear return type (type deduction fails).
    ///          For example:<br>
    ///          `m.transact<bool>([](UnsafeMapT &m) { return m.empty(); });`
    template<typename RT>
    RT transact(ModifyTxn<RT> &&f) {
        std::unique_lock lock(map_lock_);
        return f(*this);
    }

    /// @brief Overloading wrapper allowing non-returning transactions to be
    ///        written without explicitly specifying RT = void.
    void transact(ModifyTxn<void> &&f) {
        transact<void>(std::move(f));
    }

    /// @brief Type definition for a const callable returning an RT
    template<typename RT>
    using ConstTxn = std::function<RT(MapT const&)>;

    /// @brief Executes a non-modifying transaction while a shared lock is held
    ///
    /// @param f Non-modifying callable to execute
    /// @tparam RT Return value type for f
    ///
    /// @returns Value of RT returned from calling f
    ///
    /// @remarks Template parameter must be specified when passing anonymous
    ///          lambdas of unclear return type (type deduction fails).
    ///          For example:<br>
    ///          `m.transact<bool>([](const UnsafeMapT &m) { return m.empty(); });`
    template<typename RT>
    RT transact(ConstTxn<RT> &&f) const {
        std::shared_lock lock(map_lock_);
        return f(*this);
    }

    /// @brief Overloading wrapper allowing non-returning transactions to be
    ///        written without explicitly specifying RT = void.
    void transact(ConstTxn<void> &&f) const {
        transact<void>(std::move(f));
    }
    /// @}
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    /// @name Constructors
    /// Set of passthrough constructors, forwarding all arguments to the
    /// underlying map type. In the case of copy or move construction from any
    /// BaseSafeMap<T>, a lock object is added for CMCLock to hold during the
    /// copy/move operation.
    /// @{

    /// @brief Default constructor
    BaseSafeMap() : CMCLock() { }

    /// @brief Non-locking constructor taking some number of arguments
    template<typename... Args>
    BaseSafeMap(Args&&... args) : CMCLock(std::forward<Args>(args)...) { }

    /// @brief Shared-mode locking copy constructor for BaseSafeMap<*>
    /// @remarks Locks other.map_lock_ in shared mode for the duration of the
    ///          copy operation.
    template<typename OtherMapT>
    BaseSafeMap(const BaseSafeMap<OtherMapT> &other)
    : CMCLock(std::shared_lock(other.map_lock_), other) { }

    /// @brief Shared-mode locking copy constructor for BaseSafeMap<*> taking
    ///        some number of additional arguments.
    /// @remarks Locks other.map_lock_ in shared mode for the duration of the
    ///          copy operation.
    template<typename OtherMapT, typename... Args>
    BaseSafeMap(const BaseSafeMap<OtherMapT> &other, Args... args)
    : CMCLock(std::shared_lock(other.map_lock_),
            other, std::forward<Args>(args)...) { }

    /// @brief Exclusive-mode locking move constructor for BaseSafeMap<*>
    /// @remarks Locks other.map_lock_ in exclusive mode for the duration of
    ///          the mov operation.
    template<typename OtherMapT>
    BaseSafeMap(const BaseSafeMap<OtherMapT> &&other)
    : CMCLock(std::unique_lock(other.map_lock_), std::move(other)) { }

    /// @brief Exclusive-mode locking move constructor for BaseSafeMap<*>
    ///        taking some number of additional arguments.
    /// @remarks Locks other.map_lock_ in exclusive mode for the duration of
    ///          the move operation.
    template<typename OtherMapT, typename... Args>
    BaseSafeMap(const BaseSafeMap<OtherMapT> &&other, Args... args)
    : CMCLock(std::unique_lock(other.map_lock_),
            std::move(other), std::forward<Args>(args)...) { }
    /// @}
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    /// @name Map typedef pass-throughs
    /// Type definiton pass-throughs from the underlying MapT. Please refer to
    /// the documentation for your MapT for details.
    ///
    /// @note Iterator, pointer, and reference types are excluded as they are
    ///       not safe outside of locked contexts. Use transact() and
    ///       UnsafeMapT::[type] when needing to use these types.
    ///
    /// @{
    using key_type = typename MapT::key_type;
    using mapped_type = typename MapT::mapped_type;
    using value_type = typename MapT::value_type;
    using size_type = typename MapT::size_type;
    using difference_type = typename MapT::difference_type;
    //using hasher = typename MapT::hasher;
    //using key_equal = typename MapT::key_equal
    using key_compare = typename MapT::key_compare;
    using value_compare = typename MapT::value_compare;
    using allocator_type = typename MapT::allocator_type;
    //using reference = typename MapT::reference;
    //using const_reference = typename MapT::const_reference;
    //using pointer = typename MapT::pointer;
    //using const_pointer = typename MapT::const_pointer;
    //using iterator = typename MapT::iterator;
    //using const_iterator = typename MapT::const_iterator;
    //using reverse_iterator = typename MapT::reverse_iterator;
    //using const_reverse_iterator = typename MapT::const_reverse_iterator;
    using node_type = typename MapT::node_type;
    //using insert_return_type = typename MapT::insert_return_type;
    /// @}
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    /// @name Locking map method wrappers
    /// Locking wrapper methods for underlying MapT. Please refer to the
    /// documentation for your MapT for details.
    ///
    /// @note Interfaces operating on or returning Iterators, or returning
    ///       references, are not safe outside of locked contexts and so are
    ///       not included here. They can be accessed on the underlying map
    ///       through transact().
    /// @{
    BaseSafeMap<MapT>& operator=(const BaseSafeMap<MapT> &other) {
        // Other is const, so we want shared mode. Defer locking until the
        // scoped_lock attempts to lock both mutexes (provides better deadlock
        // resistance).
        std::shared_lock other_lock(other.map_lock_, std::defer_lock_t());
        // scoped_lock will see the shared_lock as if it were any other mutex,
        // but it will translate lock requests into shared-mode lock requests.
        std::scoped_lock lock(map_lock_, other_lock);
        MapT::operator=(other);
        return *this;
    }

    BaseSafeMap<MapT>& operator=(BaseSafeMap<MapT> &&other) noexcept {
        std::scoped_lock lock(map_lock_, other.map_lock_);
        MapT::operator=(std::move(other));
        return *this;
    }

    BaseSafeMap<MapT>& operator=(std::initializer_list<value_type> ilist) {
        std::unique_lock lock(map_lock_);
        MapT::operator=(ilist);
        return *this;
    }

    // skip get_allocator()

    mapped_type& at(const key_type &key) {
        std::unique_lock lock(map_lock_);
        return MapT::at(key);
    }

    const mapped_type& at(const key_type &key) const {
        std::shared_lock lock(map_lock_);
        return MapT::at(key);
    }

    mapped_type& operator[](const key_type &key) {
        std::unique_lock lock(map_lock_);
        return MapT::operator[](key);
    }

    mapped_type& operator[](key_type &&key) {
        std::unique_lock lock(map_lock_);
        return MapT::operator[](std::move(key));
    }

    // skip iterators - those should only be used through the transact interfaces
    // skip begin() and cbegin()
    // skip end() and cend()
    // skip rbegin() and crbegin()
    // skip rend() and crend()

    [[nodiscard]] bool empty() const noexcept {
        std::shared_lock lock(map_lock_);
        return MapT::empty();
    }

    size_type size() const noexcept {
        std::shared_lock lock(map_lock_);
        return MapT::size();
    }

    size_type max_size() const noexcept {
        std::shared_lock lock(map_lock_);
        return MapT::max_size();
    }

    void clear() noexcept {
        std::unique_lock lock(map_lock_);
        MapT::clear();
    }

    // skip modifiers that rely on iterators - see "skip iterators" above
    // TODO Provide static methods that can be passed to the txn interfaces
    // insert()
    // insert_range()
    // insert_or_assign()
    // emplace()
    // emplace_hint()
    // try_emplace()
    // erase()
    // extract()
    
    void swap(MapT &other) noexcept {
        std::unique_lock lock(map_lock_);
        MapT::swap(other);
    }

    // skip merge - I think it would work fine, but I'm also not sure anyone
    // is using it right now, so we'll wait until it is needed

    size_type count(const key_type &key) const {
        std::shared_lock lock(map_lock_);
        return MapT::count(key);
    }

    template<class K>
    size_type count(const K &x) const {
        std::shared_lock lock(map_lock_);
        return MapT::count(x);
    }

    // skip find - it requires iterators, so must be in a transaction

    bool contains(const key_type &key) const {
        std::shared_lock lock(map_lock_);
        return MapT::contains(key);
    }

    template<class K>
    bool contains(const K &x) const {
        std::shared_lock lock(map_lock_);
        return MapT::contains(x);
    }

    // skip equal_range - iterators
    // skip lower_bound and upper_bound - iterators
    // skip key_comp and value_comp - they can interact with iterators
    /// @}
    ///////////////////////////////////////////////////////////////////////////
};

// TODO comparison operator, std::swap(), and erase_if()
// Should these be implemented with locking, or should we just dump that into
// transactions?

/// @brief Internal details for BaseSafeMap implementation
namespace detail {
/// @brief Provides an interface for safely locking the 'other' object during
///        copy and move construction.
///
/// When locking is required by BaseSafeMap's constructors, they can pass a
/// locked std::unique_lock or std::shared_lock owning the mutex from the
/// 'other' object. This lock will be held until MapT has been initialized,
/// then checked to verify that the lock was owned throughout the process.
///
/// Otherwise, all parameters are forwarded to MapT's constructors, and public
/// inheritance of MapT provides transparante access to all other public
/// members.
///
/// @tparam MapT Type of map container to be wrapped. Could be std::map or
///              std::unordered_map, for example.
template<typename MapT>
struct CopyMoveCtorLocker : protected MapT {
    /// @brief Default constructor
    CopyMoveCtorLocker() : MapT() { }

    /// @brief Non-locking forwarding constructor
    template<typename... Args>
    CopyMoveCtorLocker(Args&&... args) : MapT(std::forward<Args>(args)...) { }

    /// @brief Exclusive-mode locking / forwarding constructor
    template<typename... Args>
    CopyMoveCtorLocker(std::unique_lock<std::shared_mutex>&& l, Args&&... args)
        : MapT(std::forward<Args>(args)...) {
            check_lock(l);
        }

    /// @brief Shared-mode locking / forwarding constructor
    template<typename... Args>
    CopyMoveCtorLocker(std::shared_lock<std::shared_mutex>&& l, Args&&... args)
        : MapT(std::forward<Args>(args)...) {
            check_lock(l);
        }

private:
    // @brief Checks that lock is not currently owned and throws an excception
    // @throws std::invalid_argument if the lock is not currently owned
    template<typename LockT>
    static void check_lock(const LockT& l) {
        if (!l.owns_lock()) {
            throw std::invalid_argument(no_owned_lock_message);
        }
    }

    /// @brief Message included in check_lock()'s thrown exception
    static constexpr std::string_view no_owned_lock_message = 
                "Attempted copy/move construction, but was unsuccessful in "
                "taking ownership of other's mutex. Cannot guarantee "
                "successful construction.";
};
} // namespace detail

} // namespace uprotocol::utils

#endif //UP_CPP_SAFE_MAP_H
