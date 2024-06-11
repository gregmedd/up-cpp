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

#include <gtest/gtest.h>
#include <up-cpp/transport/UTransport.h>

namespace {

class UTransportTests : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	UTransportTests() = default;
	~UTransportTests() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

using namespace v1 = namespace uprotocol::v1;

// TODO replace with UTransportMock
class FakeUTransport : public uprotocol::transport::UTransport {
	static v1::UStatus okStatus() {
		v1::UStatus status;
		status.set_code(v1::UCode::OK);
		return status;
	}

	static v1::UUri someUri() {
		v1::UUri uri;
		uri.set_authority_name("SomeAuth");
		uri.set_ue_id(1);
		uri.set_ue_version_major(1);
		uri.set_resource_id(0);
	}

	v1::UStatus sendImpl(const v1::UMessage& message) override {
		return okStatus();
	}

	v1::UStatus registerListenerImpl(
	    const v1::UUri& sink_filter, CallableConn&& listener,
	    std::optional<v1::UUri>&& source_filter) override {
		last_listener_ = std::move(listener);
		return okStatus();
	}

	void cleanupListener(CallableConn listener) override {
		last_cleaned_listener_ = listener;
	}

public:
	FakeUTransport() : UTransport(someUri()) {}

	size_t cleanup_count_{0};
	CallableConn last_listener_;
	CallableConn last_cleaned_listener_;
}

TEST_F(UTransportTests, CleanupGetsCalledWithListener) {
	FakeUTransport transport;
	auto handle = transport.registerCallback([]() {});

	EXPECT_TRUE(handle);
	EXPECT_TRUE(transport.last_listener_);
	EXPECT_FALSE(transport.last_cleaned_listener_);
	EXPECT_EQ(transport.cleanup_count_, 0);

	handle.reset();

	EXPECT_FALSE(handle);
	EXPECT_FALSE(transport.last_listener_);
	EXPECT_FALSE(transport.last_cleaned_listener_);
	EXPECT_EQ(transport.cleanup_count_, 1);
	EXPECT_EQ(transport.last_listener_, last_cleaned_listener_);
}

}  // namespace
