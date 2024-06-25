// SPDX-FileCopyrightText: 2024 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0

#ifndef UP_CPP_TEST_UTRANSPORTMOCK_H
#define UP_CPP_TEST_UTRANSPORTMOCK_H

#include <gmock/gmock.h>
#include <up-cpp/transport/UTransport.h>

#include <atomic>
#include <optional>

namespace uprotocol::test {

class UTransportMock : public uprotocol::transport::UTransport {
public:
	explicit UTransportMock(const v1::UUri& uuri)
	    : uprotocol::transport::UTransport(uuri) {
		++construct_count;
	}

	~UTransportMock() { ++destruct_count; }

	// Allows for sending a message to the last registered callback
	void mock_message(const uprotocol::v1::UMessage& msg) {
		ASSERT_TRUE(last_listener &&
		            "registerListener must be set before calling mock_packet");
		(*last_listener)(msg);
	}

	// Control the result code for the next send/register call (resets after)
	std::optional<v1::UStatus> next_send_status;
	std::optional<v1::UStatus> next_listen_status;

	// Tracks calls to sendImpl()
	size_t send_count{0};
	// And the parameters passed to sendImpl()
	std::optional<v1::UMessage> last_sent_message;

	// Tracks calls to registerListenerImpl()
	size_t register_count{0};
	// And the parameters passed to registerListenerImpl()
	std::optional<transport::UTransport::CallableConn> last_listener;
	std::optional<uprotocol::v1::UUri> last_source_filter;
	std::optional<v1::UUri> last_sink_filter;

	// Tracks calls to cleanupListener()
	size_t cleanup_count{0};
	// And the parameters passed to cleanupListener()
	std::optional<transport::UTransport::CallableConn> last_cleanup_listener;

	// Tracks global lifecycles of UTransportMock instances
	static inline std::atomic<size_t> construct_count;
	static inline std::atomic<size_t> destruct_count;

private:
	static v1::UStatus okStatus() {
		v1::UStatus status;
		status.set_code(v1::UCode::OK);
		return status;
	}

	[[nodiscard]] v1::UStatus sendImpl(const v1::UMessage& message) override {
		++send_count;
		last_sent_message = message;

		auto status = next_send_status ? *next_send_status : okStatus();
		if (next_send_status) next_send_status.reset();
		return status;
	}

	[[nodiscard]] v1::UStatus registerListenerImpl(
			const v1::UUri& sink_filter, CallableConn&& listener,
			std::optional<v1::UUri>&& source_filter) override {
		++register_count;
		last_listener = listener;
		last_source_filter = source_filter;
		last_sink_filter = sink_filter;

		auto status = next_listen_status ? *next_listen_status : okStatus();
		if (next_listen_status) next_listen_status.reset();
		return status;
	}

	void cleanupListener(CallableConn listener) override {
		++cleanup_count;
		last_cleanup_listener = listener;
	}
};

};  // namespace uprotocol::test

#endif  // UP_CPP_TEST_UTRANSPORTMOCK_H
