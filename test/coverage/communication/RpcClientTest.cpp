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
#include <up-cpp/datamodel/builder/Payload.h>
#include <up-cpp/datamodel/builder/Uuid.h>
#include <up-cpp/datamodel/serializer/Uuid.h>
#include <up-cpp/datamodel/validator/UMessage.h>
#include <up-cpp/datamodel/validator/UUri.h>
#include <list>

#include "UTransportMock.h"

using namespace std::chrono_literals;

namespace {

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

	void validateLastRequest(size_t expected_send_count) {
		EXPECT_TRUE(transport_->listener_);
		EXPECT_TRUE(transport_->sink_filter_ == defaultSourceUri());
		EXPECT_TRUE(transport_->source_filter_);
		EXPECT_TRUE(*(transport_->source_filter_) == methodUri());
		EXPECT_EQ(transport_->send_count_, expected_send_count);
		using namespace uprotocol::datamodel::validator;
		auto [valid_request, _] =
		    message::isValidRpcRequest(transport_->message_);
		EXPECT_TRUE(valid_request);
	}

	std::shared_ptr<uprotocol::test::UTransportMock> transport_;
};

template <typename StatusT, typename ExpectedT>
void checkErrorResponse(
    const uprotocol::communication::RpcClient::MessageOrStatus& maybe_response,
    ExpectedT expected_status) {
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

uprotocol::datamodel::builder::Payload fakePayload() {
	using namespace uprotocol::datamodel;

	auto uuid = builder::UuidBuilder::getBuilder();
	auto uuid_str = serializer::uuid::AsString::serialize(uuid.build());

	return builder::Payload(
	    std::move(uuid_str),
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);
}

///////////////////////////////////////////////////////////////////////////////
// Construction
TEST_F(RpcClientTest, CanConstructWithoutExceptions) {
	// Base parameters
	EXPECT_NO_THROW(auto client = uprotocol::communication::RpcClient(
	                    transport_, methodUri(),
	                    uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms););

	// Optional format
	EXPECT_NO_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
			10ms, uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_JSON);
		);

	// Optional permission level
	EXPECT_NO_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
			10ms, {}, 9);
		);

	// Optional permission level
	EXPECT_NO_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
			10ms, {}, {}, "Some token");
		);
}

TEST_F(RpcClientTest, ExceptionThrownWithInvalidConstructorArguments) {
	// Bad method URI
	EXPECT_THROW(
		auto uri = methodUri();
		uri.set_resource_id(0);
		auto client = uprotocol::communication::RpcClient(
			transport_, std::move(uri),
			uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);,
		uprotocol::datamodel::validator::uri::InvalidUUri);

	// Bad priority
	EXPECT_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS3,
			10ms);,
		std::out_of_range);

	// Bad ttl
	EXPECT_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
			0ms);,
		std::out_of_range);

	// Bad payload format
	EXPECT_THROW(
		auto client = uprotocol::communication::RpcClient(
			transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
			10ms, static_cast<uprotocol::v1::UPayloadFormat>(-1));,
		std::out_of_range);
}

///////////////////////////////////////////////////////////////////////////////
// RpcClient::invokeMethod()
TEST_F(RpcClientTest, InvokeFutureWithoutPayload) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	validateLastRequest(1);
	EXPECT_TRUE(transport_->message_.payload().empty());

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

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadAndFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_SOMEIP);

	EXPECT_THROW(
	    auto invoke_future = client.invokeMethod(),
	    uprotocol::datamodel::builder::UMessageBuilder::UnexpectedFormat);

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_FALSE(transport_->listener_);
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadTimeout) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	auto when_requested = std::chrono::steady_clock::now();
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(150ms);
	auto when_expired = std::chrono::steady_clock::now();

	EXPECT_GE((when_expired - when_requested), 10ms);
	EXPECT_LE((when_expired - when_requested), 2 * 10ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::DEADLINE_EXCEEDED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadListenFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->registerListener_status_.set_code(
	    uprotocol::v1::UCode::RESOURCE_EXHAUSTED);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadSendFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->send_status_.set_code(
	    uprotocol::v1::UCode::FAILED_PRECONDITION);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod());

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::FAILED_PRECONDITION);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithoutPayloadClientDestroyed) {
	std::future<uprotocol::communication::RpcClient::MessageOrStatus>
	    invoke_future;

	{
		auto client = uprotocol::communication::RpcClient(
		    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
		    10ms);

		EXPECT_NO_THROW(invoke_future = client.invokeMethod());
	}

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::CANCELLED);
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
		checkErrorResponse<uprotocol::v1::UCode>(
		    maybe_response, uprotocol::v1::UCode::PERMISSION_DENIED);
	}
}

///////////////////////////////////////////////////////////////////////////////
// RpcClient::invokeMethod(Payload)
TEST_F(RpcClientTest, InvokeFutureWithPayload) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	auto payload = fakePayload();
	auto payload_content = payload.buildCopy();

	decltype(client.invokeMethod(std::move(payload))) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(std::move(payload)));

	EXPECT_TRUE(invoke_future.valid());
	validateLastRequest(1);
	using PayloadField = uprotocol::datamodel::builder::Payload::PayloadType;
	EXPECT_EQ(transport_->message_.payload(),
	          std::get<PayloadField::Data>(payload_content));
	EXPECT_EQ(transport_->message_.attributes().payload_format(),
	          std::get<PayloadField::Format>(payload_content));

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

TEST_F(RpcClientTest, InvokeFutureWithPayloadAndFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	auto payload = fakePayload();
	auto payload_content = payload.buildCopy();

	decltype(client.invokeMethod(std::move(payload))) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(std::move(payload)));

	EXPECT_TRUE(invoke_future.valid());
	validateLastRequest(1);
	using PayloadField = uprotocol::datamodel::builder::Payload::PayloadType;
	EXPECT_EQ(transport_->message_.payload(),
	          std::get<PayloadField::Data>(payload_content));
	EXPECT_EQ(transport_->message_.attributes().payload_format(),
	          std::get<PayloadField::Format>(payload_content));

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

TEST_F(RpcClientTest, InvokeFutureWithPayloadAndWrongFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_JSON);

	EXPECT_THROW(
	    auto invoke_future = client.invokeMethod(fakePayload()),
	    uprotocol::datamodel::builder::UMessageBuilder::UnexpectedFormat);

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_FALSE(transport_->listener_);
}

TEST_F(RpcClientTest, InvokeFutureWithPayloadTimeout) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	auto when_requested = std::chrono::steady_clock::now();
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(fakePayload()));

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(150ms);
	auto when_expired = std::chrono::steady_clock::now();

	EXPECT_GE((when_expired - when_requested), 10ms);
	EXPECT_LE((when_expired - when_requested), 2 * 10ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::DEADLINE_EXCEEDED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithPayloadListenFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->registerListener_status_.set_code(
	    uprotocol::v1::UCode::RESOURCE_EXHAUSTED);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(fakePayload()));

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithPayloadSendFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->send_status_.set_code(
	    uprotocol::v1::UCode::FAILED_PRECONDITION);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(fakePayload()));

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::FAILED_PRECONDITION);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithPayloadClientDestroyed) {
	std::future<uprotocol::communication::RpcClient::MessageOrStatus>
	    invoke_future;

	{
		auto client = uprotocol::communication::RpcClient(
		    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
		    10ms);

		EXPECT_NO_THROW(invoke_future = client.invokeMethod(fakePayload()));
	}

	EXPECT_TRUE(invoke_future.valid());
	auto is_ready = invoke_future.wait_for(0ms);

	EXPECT_EQ(is_ready, std::future_status::ready);
	if (is_ready == std::future_status::ready) {
		auto maybe_response = invoke_future.get();
		checkErrorResponse<uprotocol::v1::UStatus>(
		    maybe_response, uprotocol::v1::UCode::CANCELLED);
	}
}

TEST_F(RpcClientTest, InvokeFutureWithPayloadCommstatus) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	decltype(client.invokeMethod()) invoke_future;
	EXPECT_NO_THROW(invoke_future = client.invokeMethod(fakePayload()));

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
		checkErrorResponse<uprotocol::v1::UCode>(
		    maybe_response, uprotocol::v1::UCode::PERMISSION_DENIED);
	}
}

///////////////////////////////////////////////////////////////////////////////
// RpcClient::invokeMethod(Callback)
TEST_F(RpcClientTest, InvokeCallbackWithoutPayload) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	bool callback_called = false;
	uprotocol::v1::UMessage received_response;

	EXPECT_NO_THROW(client.invokeMethod(
	    [this, &callback_called, &received_response](auto maybe_response) {
		    callback_called = true;
		    EXPECT_TRUE(maybe_response);
		    received_response = std::move(maybe_response).value();
	    }));

	validateLastRequest(1);
	EXPECT_TRUE(transport_->message_.payload().empty());

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(callback_called);
	EXPECT_TRUE(response == received_response);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadAndFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_SOMEIP);

	EXPECT_THROW(
	    client.invokeMethod([](auto) {}),
	    uprotocol::datamodel::builder::UMessageBuilder::UnexpectedFormat);

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_FALSE(transport_->listener_);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadTimeout) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	bool callback_called = false;
	std::condition_variable callback_event;
	auto when_requested = std::chrono::steady_clock::now();

	EXPECT_NO_THROW(client.invokeMethod(
	    [this, &callback_called, &callback_event](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::DEADLINE_EXCEEDED);
		    callback_event.notify_all();
	    }));

	std::mutex mtx;
	std::unique_lock lock(mtx);
	callback_called = callback_event.wait_for(
	    lock, 150ms, [&callback_called]() { return callback_called; });
	auto when_expired = std::chrono::steady_clock::now();

	EXPECT_GE((when_expired - when_requested), 10ms);
	EXPECT_LE((when_expired - when_requested), 2 * 10ms);

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadListenFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->registerListener_status_.set_code(
	    uprotocol::v1::UCode::RESOURCE_EXHAUSTED);

	bool callback_called = false;

	EXPECT_NO_THROW(
	    client.invokeMethod([this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	    }));

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadSendFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->send_status_.set_code(
	    uprotocol::v1::UCode::FAILED_PRECONDITION);

	bool callback_called = false;

	EXPECT_NO_THROW(
	    client.invokeMethod([this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::FAILED_PRECONDITION);
	    }));

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadClientDestroyed) {
	std::future<uprotocol::communication::RpcClient::MessageOrStatus>
	    invoke_future;

	bool callback_called = false;

	{
		auto client = uprotocol::communication::RpcClient(
		    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
		    10ms);

		EXPECT_NO_THROW(
		    client.invokeMethod([this, &callback_called](auto maybe_response) {
			    callback_called = true;
			    checkErrorResponse<uprotocol::v1::UStatus>(
			        maybe_response, uprotocol::v1::UCode::CANCELLED);
		    }));
	}

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithoutPayloadCommstatus) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	bool callback_called = false;

	EXPECT_NO_THROW(
	    client.invokeMethod([this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UCode>(
		        maybe_response, uprotocol::v1::UCode::PERMISSION_DENIED);
	    }));

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	response_builder.withCommStatus(uprotocol::v1::UCode::PERMISSION_DENIED);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(callback_called);
}

///////////////////////////////////////////////////////////////////////////////
// RpcClient::invokeMethod(Payload, Callback)
TEST_F(RpcClientTest, InvokeCallbackWithPayload) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	auto payload = fakePayload();
	auto payload_content = payload.buildCopy();

	bool callback_called = false;
	uprotocol::v1::UMessage received_response;

	EXPECT_NO_THROW(client.invokeMethod(
	    std::move(payload),
	    [this, &callback_called, &received_response](auto maybe_response) {
		    callback_called = true;
		    EXPECT_TRUE(maybe_response);
		    received_response = std::move(maybe_response).value();
	    }));

	validateLastRequest(1);
	using PayloadField = uprotocol::datamodel::builder::Payload::PayloadType;
	EXPECT_EQ(transport_->message_.payload(),
	          std::get<PayloadField::Data>(payload_content));
	EXPECT_EQ(transport_->message_.attributes().payload_format(),
	          std::get<PayloadField::Format>(payload_content));

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(callback_called);
	EXPECT_TRUE(response == received_response);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadAndFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_TEXT);

	auto payload = fakePayload();
	auto payload_content = payload.buildCopy();

	bool callback_called = false;
	uprotocol::v1::UMessage received_response;

	EXPECT_NO_THROW(client.invokeMethod(
	    std::move(payload),
	    [this, &callback_called, &received_response](auto maybe_response) {
		    callback_called = true;
		    EXPECT_TRUE(maybe_response);
		    received_response = std::move(maybe_response).value();
	    }));

	validateLastRequest(1);
	using PayloadField = uprotocol::datamodel::builder::Payload::PayloadType;
	EXPECT_EQ(transport_->message_.payload(),
	          std::get<PayloadField::Data>(payload_content));
	EXPECT_EQ(transport_->message_.attributes().payload_format(),
	          std::get<PayloadField::Format>(payload_content));

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(callback_called);
	EXPECT_TRUE(response == received_response);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadAndWrongFormatSet) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms,
	    uprotocol::v1::UPayloadFormat::UPAYLOAD_FORMAT_JSON);

	EXPECT_THROW(
	    client.invokeMethod(fakePayload(), [](auto) {}),
	    uprotocol::datamodel::builder::UMessageBuilder::UnexpectedFormat);

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_FALSE(transport_->listener_);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadTimeout) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	bool callback_called = false;
	std::condition_variable callback_event;

	auto when_requested = std::chrono::steady_clock::now();
	EXPECT_NO_THROW(client.invokeMethod(
	    fakePayload(),
	    [this, &callback_called, &callback_event](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::DEADLINE_EXCEEDED);
		    callback_event.notify_all();
	    }));

	std::mutex mtx;
	std::unique_lock lock(mtx);
	callback_called = callback_event.wait_for(
	    lock, 150ms, [&callback_called]() { return callback_called; });
	auto when_expired = std::chrono::steady_clock::now();

	EXPECT_GE((when_expired - when_requested), 10ms);
	EXPECT_LE((when_expired - when_requested), 2 * 10ms);

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadListenFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->registerListener_status_.set_code(
	    uprotocol::v1::UCode::RESOURCE_EXHAUSTED);

	bool callback_called = false;

	EXPECT_NO_THROW(client.invokeMethod(
	    fakePayload(), [this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	    }));

	EXPECT_EQ(transport_->send_count_, 0);
	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadSendFail) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	transport_->send_status_.set_code(
	    uprotocol::v1::UCode::FAILED_PRECONDITION);

	bool callback_called = false;

	EXPECT_NO_THROW(client.invokeMethod(
	    fakePayload(), [this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UStatus>(
		        maybe_response, uprotocol::v1::UCode::FAILED_PRECONDITION);
	    }));

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadClientDestroyed) {
	std::future<uprotocol::communication::RpcClient::MessageOrStatus>
	    invoke_future;

	bool callback_called = false;

	{
		auto client = uprotocol::communication::RpcClient(
		    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4,
		    10ms);

		EXPECT_NO_THROW(client.invokeMethod(
		    fakePayload(), [this, &callback_called](auto maybe_response) {
			    callback_called = true;
			    checkErrorResponse<uprotocol::v1::UStatus>(
			        maybe_response, uprotocol::v1::UCode::CANCELLED);
		    }));
	}

	EXPECT_TRUE(callback_called);
}

TEST_F(RpcClientTest, InvokeCallbackWithPayloadCommstatus) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 10ms);

	bool callback_called = false;

	EXPECT_NO_THROW(client.invokeMethod(
	    fakePayload(), [this, &callback_called](auto maybe_response) {
		    callback_called = true;
		    checkErrorResponse<uprotocol::v1::UCode>(
		        maybe_response, uprotocol::v1::UCode::PERMISSION_DENIED);
	    }));

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;
	auto response_builder = UMessageBuilder::response(transport_->message_);
	response_builder.withCommStatus(uprotocol::v1::UCode::PERMISSION_DENIED);
	auto response = response_builder.build();
	EXPECT_NO_THROW(transport_->mockMessage(response));

	EXPECT_TRUE(callback_called);
}

///////////////////////////////////////////////////////////////////////////////
// Usecases
TEST_F(RpcClientTest, MultipleSimultaneousInvocations) {
	auto client = uprotocol::communication::RpcClient(
	    transport_, methodUri(), uprotocol::v1::UPriority::UPRIORITY_CS4, 250ms);

	std::list<decltype(client.invokeMethod())> futures;
	std::list<std::decay_t<decltype(transport_->listener_.value())>> callables;
	std::list<uprotocol::v1::UMessage> requests;

	futures.push_back(client.invokeMethod());
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	futures.push_back(client.invokeMethod(fakePayload()));
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	futures.push_back(client.invokeMethod());
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	futures.push_back(client.invokeMethod(fakePayload()));
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	std::vector<uprotocol::transport::UTransport::ListenHandle> handles;

	int callback_count = 0;
	auto callback = [&callback_count](auto){ ++callback_count; };

	client.invokeMethod(callback);
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	client.invokeMethod(fakePayload(), callback);
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	client.invokeMethod(callback);
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	client.invokeMethod(fakePayload(), callback);
	callables.push_back(transport_->listener_.value());
	requests.push_back(transport_->message_);

	auto readyFutures = [&futures]() {
		size_t ready = 0;
		for (auto& future : futures) {
			auto is_ready = future.wait_for(0ms);
			if (is_ready == std::future_status::ready) {
				++ready;
			}
		}
		return ready;
	};

	EXPECT_EQ(callback_count, 0);
	EXPECT_EQ(readyFutures(), 0);

	using UMessageBuilder = uprotocol::datamodel::builder::UMessageBuilder;

	auto deliverMessage = [&callables](uprotocol::v1::UMessage&& message) {
		for (auto& callable : callables) {
			callable(std::move(message));
		}
	};

	deliverMessage(UMessageBuilder::response(requests.front()).build());
	deliverMessage(UMessageBuilder::response(requests.back()).build());

	EXPECT_EQ(callback_count, 1);
	EXPECT_EQ(readyFutures(), 1);
	EXPECT_EQ(futures.front().wait_for(0ms), std::future_status::ready);

	requests.pop_front();
	requests.pop_back();

	deliverMessage(UMessageBuilder::response(requests.front()).build());
	deliverMessage(UMessageBuilder::response(requests.back()).build());
	requests.pop_front();
	requests.pop_back();
	deliverMessage(UMessageBuilder::response(requests.front()).build());
	deliverMessage(UMessageBuilder::response(requests.back()).build());

	EXPECT_EQ(callback_count, 3);
	EXPECT_EQ(readyFutures(), 3);

	// Intentionally leaving a couple pending requests to discard
}

}  // namespace
