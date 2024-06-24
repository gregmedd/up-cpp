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

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <up-cpp/communication/RpcClient.h>
#include <up-cpp/datamodel/validator/UMessage.h>

#include "UTransportMock.h"

using namespace std::chrono_literals;

namespace {

class RpcClientTest : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {
		transport_ = std::make_shared<uprotocol::test::UTransportMock>(
		    defaultSourceUri());
	}

	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	RpcClientTest() = default;
	~RpcClientTest() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {
		// Gives clean valgrind output. Protobufs isn't losing track of the
		// memory, but it also doesn't automatically free its allocated memory
		// when the application exits.
		google::protobuf::ShutdownProtobufLibrary();
	}

	static uprotocol::v1::UUri methodUri(const std::string& auth = "TestAuth",
	                                     uint16_t ue_id = 0x8000,
	                                     uint16_t ue_instance = 1,
	                                     uint16_t ue_version_major = 1,
	                                     uint16_t resource_id = 1) {
		uprotocol::v1::UUri uri;
		uri.set_authority_name(auth);
		uri.set_ue_id(static_cast<uint32_t>(ue_instance) << 16 |
		              static_cast<uint32_t>(ue_id));
		uri.set_ue_version_major(ue_version_major);
		uri.set_resource_id(resource_id);
		return uri;
	}

	static uprotocol::v1::UUri defaultSourceUri() {
		auto uri = methodUri();
		uri.set_resource_id(0);
		return uri;
	}

	std::shared_ptr<uprotocol::test::UTransportMock> transport_;
};

bool operator==(const uprotocol::v1::UUri& lhs,
                const uprotocol::v1::UUri& rhs) {
	using namespace google::protobuf::util;
	return MessageDifferencer::Equals(lhs, rhs);
}

bool operator==(const uprotocol::v1::UMessage& lhs,
                const uprotocol::v1::UMessage& rhs) {
	using namespace google::protobuf::util;
	return MessageDifferencer::Equals(lhs, rhs);
}

bool operator==(const uprotocol::v1::UStatus& lhs,
                const uprotocol::v1::UStatus& rhs) {
	using namespace google::protobuf::util;
	return MessageDifferencer::Equals(lhs, rhs);
}

bool operator==(const uprotocol::v1::UStatus& lhs,
                const uprotocol::v1::UCode& rhs) {
	return lhs.code() == rhs;
}

TEST_F(RpcClientTest, CanConstructWithoutExceptions) {
	EXPECT_NO_THROW(auto client = uprotocol::communication::RpcClient(
	                    transport_, methodUri(),
	                    uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms););
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayload) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	EXPECT_TRUE(transport_->listener_);
	EXPECT_TRUE(transport_->sink_filter_ == defaultSourceUri());
	EXPECT_TRUE(transport_->source_filter_);
	EXPECT_TRUE(*(transport_->source_filter_) == methodUri());
	EXPECT_EQ(transport_->send_count_, 1);
	EXPECT_TRUE(transport_->message_.payload().empty());
	using namespace uprotocol::datamodel::validator;
	auto [valid_request, _] = message::isValidRpcRequest(transport_->message_);
	EXPECT_TRUE(valid_request);

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		EXPECT_TRUE(maybe_response);
		EXPECT_TRUE(response == maybe_response.value());
	}
}

template <typename StatusT, typename ExpectedT>
void checkErrorResponse(const uprotocol::communication::RpcClient::MessageOrStatus& maybe_response, ExpectedT expected_status) {
	EXPECT_FALSE(maybe_response);
	if (!maybe_response) {
		auto& some_status = maybe_response.error();
		EXPECT_TRUE(std::holds_alternative<StatusT>(some_status));
		if (std::holds_alternative<StatusT>(some_status)) {
			auto& status = std::get<StatusT>(some_status);
			EXPECT_TRUE(status == expected_status);
		}
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadTimeout) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(150ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(maybe_response, uprotocol::v1::UCode::DEADLINE_EXCEEDED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadListenFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->registerListener_status_.set_code(uprotocol::v1::UCode::RESOURCE_EXHAUSTED);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(maybe_response, uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadSendFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->send_status_.set_code(uprotocol::v1::UCode::FAILED_PRECONDITION);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(maybe_response, uprotocol::v1::UCode::FAILED_PRECONDITION);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadClientDestroyed) {
	std::future<uprotocol::communication::RpcClient::MessageOrStatus> invoke_future;

	{
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

		EXPECT_NO_THROW(invoke_future = client.invokeMethod());
	}

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(maybe_response, uprotocol::v1::UCode::CANCELLED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadCommstatus) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	response_builder.withCommStatus(uprotocol::v1::UCode::PERMISSION_DENIED);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UCode>(maybe_response, uprotocol::v1::UCode::PERMISSION_DENIED);
	}
}

}  // namespace
