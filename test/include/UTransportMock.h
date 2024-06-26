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

namespace uprotocol::test {

class UTransportMock : public uprotocol::transport::UTransport {
public:
	explicit UTransportMock(const v1::UUri& uuri)
	    : uprotocol::transport::UTransport(uuri), send_count_(0) {}

	void mockMessage(const uprotocol::v1::UMessage& msg) {
		ASSERT_TRUE(listener_ &&
		            "registerListener must be set before calling mock_packet");
		(*listener_)(msg);
	}

	size_t send_count_;
	uprotocol::v1::UStatus send_status_;
	std::optional<uprotocol::utils::callbacks::CallerHandle<
	    void, uprotocol::v1::UMessage const&>>
	    listener_;
	std::optional<uprotocol::utils::callbacks::CallerHandle<
	    void, uprotocol::v1::UMessage const&>>
	    cleanup_listener_;
	std::optional<uprotocol::v1::UUri> source_filter_;
	v1::UUri sink_filter_;
	v1::UMessage message_;

private:
	[[nodiscard]] v1::UStatus sendImpl(const v1::UMessage& message) override {
		v1::UStatus retval;
		message_ = message;
		send_count_++;
		return send_status_;
	}

	[[nodiscard]] v1::UStatus registerListenerImpl(
	    const v1::UUri& sink_filter, CallableConn&& listener,
	    std::optional<v1::UUri>&& source_filter) override {
		listener_ = listener;
		source_filter_ = source_filter;
		sink_filter_ = sink_filter;
		v1::UStatus retval;
		retval.set_code(v1::UCode::OK);
		return retval;
	}

	void cleanupListener(CallableConn listener) override {
		cleanup_listener_ = listener;
	}
};

};  // namespace uprotocol::test

#endif  // UP_CPP_TEST_UTRANSPORTMOCK_H
