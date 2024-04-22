/*
 * Copyright (c) 2023 General Motors GTO LLC
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
 * SPDX-FileCopyrightText: 2023 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CYCLIC_QUEUE_HPP__
#define __CYCLIC_QUEUE_HPP__

#include <queue>
#include <mutex>
#include <condition_variable>

namespace uprotocol::utils {

	template<typename T>
	class CyclicQueue final
	{
	public:
		explicit CyclicQueue(
			const size_t maxSize,
			const std::chrono::milliseconds milliseconds);

		CyclicQueue(const CyclicQueue&) = delete;
		CyclicQueue &operator=(const CyclicQueue&) = delete;

		virtual ~CyclicQueue() = default;

		bool push(T& data) noexcept;

		bool isFull(void) const noexcept;

		bool isEmpty(void) const noexcept;

		bool waitPop(T& popped_value) noexcept;

		size_t size(void) const noexcept;

		void clear(void) noexcept;

	private:
		static constexpr std::chrono::milliseconds DefaultPopQueueTimeoutMilli { 5U };

		size_t queueMaxSize_;
		mutable std::mutex mutex_;
		std::condition_variable conditionVariable_;
		std::chrono::milliseconds milliseconds_ { DefaultPopQueueTimeoutMilli };
		std::queue<T> queue_;
	};
}
#endif // __CYCLIC_QUEUE_HPP__
