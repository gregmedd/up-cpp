/*
 * Copyright (c) 2024 General Motors GTO LLC
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2024 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SAFEMAP_H
#define SAFEMAP_H

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using namespace std;

namespace uprotocol::utils {

template<typename MapT>
class BaseSafeMap;

template<typename T, typename Key>
using SafeMap = BaseSafeMap<std::map<T, Key>>;

template<typename T, typename Key>
using SafeUnorderedMap = BaseSafeMap<std::unordered_map<T, Key>>;

namespace detail {
struct CopyConstructLocker {
    CopyConstructLocker() { }
    CopyConstructLocker(std::shared_mutex &m) : lock(m) { }
    std::unique_lock<std::shared_mutex> lock;
};
}

template<typename MapT>
class BaseSafeMap : private detail::CopyConstructLocker, protected MapT {
    mutable std::shared_mutex map_lock_;
    using SafeMapT = BaseSafeMap<MapT>;

public:
    BaseSafeMap() : MapT() { }

    BaseSafeMap(SafeMapT const &other) : CopyConstructLocker(other.map_lock_), MapT(other) {
        CopyConstructLocker::lock.unlock();
    }

    BaseSafeMap(SafeMapT &&other) : CopyConstructLocker(other.map_lock_), MapT(std::move(other)), map_lock_(std::move(other.map_lock_)) {
        CopyConstructLocker::lock.unlock();
    }

    BaseSafeMap(std::initializer_list<typename MapT::value_type> ilist) : MapT(ilist) { }
};

} // namespace uprotocol::utils

#endif //SAFEMAP_H
