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

#include <up-cpp/datamodel/validator/UUri.h>
#include <up-cpp/datamodel/builder/UMessage.h>

#include <UTransportMock.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <memory>
#include <random>
#include <sstream>

static std::random_device random_dev;
static std::mt19937 random_gen(random_dev());
static std::uniform_int_distribution<int> char_dist('A', 'z');

std::string get_random_string(size_t max_len = 32) {
	std::uniform_int_distribution<int> len_dist(1, max_len);
	size_t len = len_dist(random_gen);
	std::string retval;
	retval.reserve(len);
	for (size_t i = 0; i < len; i++)
		retval += char_dist(random_gen);
	return retval;
}

int get_random_int(int mn = 0, int mx = 100) {
	std::uniform_int_distribution<int> int_dist(mn, mx);
	return int_dist(random_gen);
}

uprotocol::v1::UUID make_uuid() {
	uint64_t timestamp =
	    std::chrono::duration_cast<std::chrono::milliseconds>(
	        std::chrono::system_clock::now().time_since_epoch())
	        .count();
	uprotocol::v1::UUID id;
	id.set_msb((timestamp << 16) | (8ULL << 12) |
	            (0x123ULL));  // version 8 ; counter = 0x123
	id.set_lsb((2ULL << 62) | (0xFFFFFFFFFFFFULL));  // variant 10
	return id;
}

namespace {

class TestMockUTransport : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	TestMockUTransport() = default;
	~TestMockUTransport() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}

	static std::shared_ptr<uprotocol::test::UTransportMock> getNewTransport(
			std::optional<uprotocol::v1::UUri> defaultUri = {}) {
		return std::make_shared<uprotocol::test::UTransportMock>(
				defaultUri.value_or(getRandomUri(true)));
	}

	static uprotocol::v1::UUri getRandomUri(bool as_default = false) {
		auto isValid = [](const auto& uuri) {
			auto [valid, _] = uprotocol::datamodel::validator::uri::isValid(uuri);
			return valid;
		};

		uprotocol::v1::UUri uuri;

		while (!isValid(uuri)) {
			uuri.set_authority_name(get_random_string());
			auto ue_instance = get_random_int(1, 0xFFFF);
			auto ue_id = as_default ? get_random_int(0x8000, 0xFFFE) : get_random_int(1, 0xFFFE);
			uuri.set_ue_id(((ue_instance << 16) & 0xFFFF0000) | (ue_id & 0xFFFE));
			uuri.set_ue_version_major(get_random_int(1,0xFFFE));
			uuri.set_resource_id(as_default ? 0 : get_random_int(1, 0xFFFE));
		}

		return uuri;
	}

	uprotocol::v1::UMessageType typeFromUri(const uprotocol::v1::UUri& uuri) {
		using namespace uprotocol::datamodel::validator;

		auto [isRpc, a] = uri::isValidRpcMethod(uuri);
		if (isRpc) {
			return uprotocol::v1::UMessageType::UMESSAGE_TYPE_REQUEST;
		}
		auto x = get_random_int(0, 1);
		if(x == 0) {
			auto [isPublish, b] = uri::isValidPublishTopic(uuri);
			if (isPublish) {
				return uprotocol::v1::UMessageType::UMESSAGE_TYPE_PUBLISH;
			}
		}
		auto [isNotify, c] = uri::isValidNotification(uuri);
		if (isNotify) {
			return uprotocol::v1::UMessageType::UMESSAGE_TYPE_NOTIFICATION;
		}
		return uprotocol::v1::UMessageType::UMESSAGE_TYPE_UNSPECIFIED;
	}
};

using MsgDiff = google::protobuf::util::MessageDifferencer;

TEST_F(TestMockUTransport, ConstructDestroy) {
	auto def_src_uri = getRandomUri(true);
	auto transport = getNewTransport(def_src_uri);

	EXPECT_NE(nullptr, transport);
	EXPECT_EQ(transport->construct_count, 1);
	EXPECT_EQ(transport->destruct_count, 0);
	EXPECT_TRUE(MsgDiff::Equals(def_src_uri, transport->getDefaultSource()));

	transport.reset();
	EXPECT_EQ(transport->construct_count, 1);
	EXPECT_EQ(transport->destruct_count, 1);
}

TEST_F(TestMockUTransport, SendMessage) {
	auto transport = getNewTransport();
	EXPECT_NE(nullptr, transport);

	constexpr size_t max_count = 1000;
	for (size_t i = 0; i < max_count; ++i) {
		auto sink = getRandomUri();
		auto type = typeFromUri(sink);
		while (type == uprotocol::v1::UMessageType::UMESSAGE_TYPE_UNSPECIFIED) {
			sink = getRandomUri();
			type = typeFromUri(sink);
		}
	   
		uprotocol::v1::UMessage msg;
		msg.set_payload(get_random_string(1400));

		auto& attr = *(msg.mutable_attributes());
		attr.set_type(type);
		*(attr.mutable_id()) = make_uuid();
		if (type != uprotocol::v1::UMessageType::UMESSAGE_TYPE_PUBLISH) {
			*(attr.mutable_source()) = transport->getDefaultSource();
			*(attr.mutable_sink()) = sink;
		} else {
			*(attr.mutable_source()) = sink;
		}
		attr.set_priority(uprotocol::v1::UPriority::UPRIORITY_CS4);
		attr.set_payload_format(uprotocol::v1::UPAYLOAD_FORMAT_TEXT);
		attr.set_ttl(1000);

		uprotocol::v1::UStatus status;
		status.set_code(static_cast<uprotocol::v1::UCode>(15 - (i % 16)));
		status.set_message(get_random_string());
		transport->next_send_status = status;

		decltype(transport->send(msg)) result;
		EXPECT_NO_THROW(result = transport->send(msg));

		EXPECT_EQ(i + 1, transport->send_count);
		EXPECT_TRUE(MsgDiff::Equals(result, status));
		EXPECT_TRUE(transport->last_sent_message);
		EXPECT_TRUE(MsgDiff::Equals(msg, *(transport->last_sent_message)));
	}
}

TEST_F(TestMockUTransport, RegisterListener) {
	auto transport = getNewTransport();
	EXPECT_NE(nullptr, transport);

	uprotocol::v1::UMessage last_cb_msg;
	size_t cb_count = 0;
	auto action = [&last_cb_msg, &cb_count](const uprotocol::v1::UMessage& msg) {
		last_cb_msg = msg;
		++cb_count;
	};

	auto sink_filter = getRandomUri();
	auto source_filter = getRandomUri();

	EXPECT_EQ(transport->register_count, 0);

	auto maybeHandle = transport->registerListener(
			sink_filter, std::move(action), source_filter);

	EXPECT_EQ(transport->register_count, 1);
	EXPECT_TRUE(transport->last_listener);
	EXPECT_TRUE(MsgDiff::Equals(sink_filter, *(transport->last_sink_filter)));
	EXPECT_TRUE(transport->last_source_filter);
	EXPECT_TRUE(MsgDiff::Equals(source_filter, *(transport->last_source_filter)));

	EXPECT_TRUE(maybeHandle.has_value());
	auto handle = std::move(maybeHandle).value();
	EXPECT_TRUE(handle);

	constexpr size_t max_count = 1000;
	for (auto i = 0; i < max_count; i++) {
		uprotocol::v1::UMessage msg;
		*(msg.mutable_attributes()->mutable_id()) = make_uuid();
		msg.set_payload(get_random_string(1400));
		transport->mock_message(msg);
		EXPECT_EQ(i + 1, cb_count);
		EXPECT_TRUE(MsgDiff::Equals(msg, last_cb_msg));
	}

	EXPECT_FALSE(transport->last_cleanup_listener);
	EXPECT_EQ(transport->cleanup_count, 0);

	handle.reset();

	EXPECT_EQ(transport->cleanup_count, 1);
	EXPECT_TRUE(transport->last_cleanup_listener);
	EXPECT_EQ(*(transport->last_cleanup_listener), *(transport->last_listener));
}

TEST_F(TestMockUTransport, RegisterListenerNotOk) {
	auto transport = getNewTransport();
	EXPECT_NE(nullptr, transport);

	auto sink_filter = getRandomUri();
	auto source_filter = getRandomUri();

	auto action = [&](const auto& x) { EXPECT_EQ("", "Should not call"); };

	uprotocol::v1::UStatus status;
	status.set_code(uprotocol::v1::UCode::RESOURCE_EXHAUSTED);
	status.set_message("Pretend resources have been exhausted");
	transport->next_listen_status = status;

	EXPECT_EQ(transport->register_count, 0);

	auto maybeHandle = transport->registerListener(
			sink_filter, std::move(action), source_filter);

	EXPECT_EQ(transport->register_count, 1);

	EXPECT_FALSE(maybeHandle.has_value());
	EXPECT_TRUE(MsgDiff::Equals(maybeHandle.error(), status));
	// A listener was passed, but it is not connected anymore
	EXPECT_TRUE(transport->last_listener);
	EXPECT_FALSE(*(transport->last_listener));
}

TEST_F(TestMockUTransport, BalancedCreateDestroy) {
	using namespace uprotocol::test;
	EXPECT_EQ(UTransportMock::construct_count, UTransportMock::destruct_count);
}

}  // namespace
