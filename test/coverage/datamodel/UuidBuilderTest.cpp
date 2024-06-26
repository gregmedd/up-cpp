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

#include "up-cpp/datamodel/builder/Uuid.h"
#include "up-cpp/datamodel/constants/UuidConstants.h"

namespace {

using namespace uprotocol::datamodel::builder;
using namespace uprotocol::datamodel;

class TestUuidBuilder : public testing::Test {
protected:
	void SetUp() override {}
	void TearDown() override {}

	TestUuidBuilder() = default;
	~TestUuidBuilder() = default;

	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

// Test getBuilder
TEST(UuidBuilderTest, GetBuilder) {
	auto builder = UuidBuilder::getBuilder();
	auto uuid = builder.build();

	EXPECT_TRUE(uuid.msb());
	EXPECT_TRUE(uuid.lsb());
}

// Test GetTestBuilder
TEST(UuidBuilderTest, GetTestBuilder) {
	auto builder = UuidBuilder::getTestBuilder();
	auto uuid = builder.build();

	EXPECT_TRUE(uuid.msb());
	EXPECT_TRUE(uuid.lsb());
}

// Test TestBuilder with time source
TEST(UuidBuilderTest, WithTimeSource) {
	auto fixed_time = std::chrono::system_clock::from_time_t(1234567890);
	auto fixed_time_ms =
	    std::chrono::time_point_cast<std::chrono::milliseconds>(fixed_time);
	auto builder = UuidBuilder::getTestBuilder().withTimeSource(
	    [fixed_time]() { return fixed_time; });
	auto uuid = builder.build();

	EXPECT_EQ(uuid.msb() >> UUID_TIMESTAMP_SHIFT,
	          fixed_time_ms.time_since_epoch().count());
}

// Test RandomSource
TEST(UuidBuilderTest, WithRandomSource) {
	auto fixed_random = 0x1234567890ABCDEF;
	auto builder = UuidBuilder::getTestBuilder().withRandomSource(
	    [fixed_random]() { return fixed_random; });
	auto uuid = builder.build();

	EXPECT_EQ(uuid.lsb() & UUID_RANDOM_MASK, fixed_random & UUID_RANDOM_MASK);
}

// Test independent state
TEST(UuidBuilderTest, WithIndependentState) {
	auto builder1 = UuidBuilder::getTestBuilder().withIndependentState();
	auto builder2 = UuidBuilder::getTestBuilder().withIndependentState();

	// Generate multiple UUIDs from each builder
	const int numUuids = 100;
	std::vector<uprotocol::v1::UUID> uuids1;
	std::vector<uprotocol::v1::UUID> uuids2;

	for (int i = 0; i < numUuids; ++i) {
		uuids1.push_back(builder1.build());
		uuids2.push_back(builder2.build());
	}

	// Check that UUIDs generated by different builders have different LSBs
	for (int i = 0; i < numUuids; ++i) {
		EXPECT_NE(uuids1[i].lsb(), uuids2[i].lsb());
	}

	// Check that UUIDs generated by the same builder have the same LSBs
	for (int i = 1; i < numUuids; ++i) {
		EXPECT_EQ(uuids1[i].lsb(), uuids1[0].lsb());
		EXPECT_EQ(uuids2[i].lsb(), uuids2[0].lsb());
	}

	// Check that the timestamps are monotonically increasing within each
	// builder
	for (int i = 1; i < numUuids; ++i) {
		EXPECT_GE(uuids1[i].msb(), uuids1[i - 1].msb());
		EXPECT_GE(uuids2[i].msb(), uuids2[i - 1].msb());
	}

	// Check that the counters are incrementing within each builder
	for (int i = 1; i < numUuids; ++i) {
		uint16_t counter1_prev = uuids1[i - 1].msb() & UUID_COUNTER_MASK;
		uint16_t counter1_curr = uuids1[i].msb() & UUID_COUNTER_MASK;
		EXPECT_EQ(counter1_curr, (counter1_prev + 1) % 4096);

		uint16_t counter2_prev = uuids2[i - 1].msb() & UUID_COUNTER_MASK;
		uint16_t counter2_curr = uuids2[i].msb() & UUID_COUNTER_MASK;
		EXPECT_EQ(counter2_curr, (counter2_prev + 1) % 4096);
	}
}

// Test exception thrown
TEST(UuidBuilderTest, TestModeOnly) {
	auto builder = UuidBuilder::getBuilder();

	EXPECT_THROW(builder.withTimeSource(
	                 []() { return std::chrono::system_clock::now(); }),
	             std::domain_error);
	EXPECT_THROW(builder.withRandomSource([]() { return 0x1234567890ABCDEF; }),
	             std::domain_error);
	EXPECT_THROW(builder.withIndependentState(), std::domain_error);
}

// Test version and variant
TEST_F(TestUuidBuilder, CheckVersionAndVariant) {
	auto builder = UuidBuilder::getBuilder();
	auto uuid = builder.build();

	EXPECT_EQ((uuid.msb() >> UUID_VERSION_SHIFT) & UUID_VERSION_MASK,
	          UUID_VERSION_8);
	EXPECT_EQ((uuid.lsb() >> UUID_VARIANT_SHIFT) & UUID_VARIANT_MASK,
	          UUID_VARIANT_RFC4122);
}

// Test counter increments
TEST_F(TestUuidBuilder, CounterIncrements) {
	auto builder = UuidBuilder::getBuilder();
	auto uuid1 = builder.build();
	auto uuid2 = builder.build();

	uint16_t counter1 = uuid1.msb() & UUID_COUNTER_MASK;
	uint16_t counter2 = uuid2.msb() & UUID_COUNTER_MASK;

	EXPECT_EQ(counter2, counter1 + 1);
}

// Test counter reset at millisecond
TEST(UuidBuilderTest, CounterResetAtTimestampTick) {
	auto builder = UuidBuilder::getTestBuilder();

	// Generate a UUID
	auto uuid1 = builder.build();

	// Extract the counter value from the UUID
	uint16_t counter1 = (uuid1.msb() & UUID_COUNTER_MASK);

	// Check counter value is within the expected range
	EXPECT_GE(counter1, 0);
	EXPECT_LE(counter1, 4095);

	// Advance the timestamp by 1 millisecond
	auto timestamp =
	    std::chrono::system_clock::now() + std::chrono::milliseconds(1);
	builder.withTimeSource([timestamp]() { return timestamp; });

	// Generate another UUID with the new timestamp
	auto uuid2 = builder.build();

	// Extract the counter value from the new UUID
	uint16_t counter2 = (uuid2.msb() & UUID_COUNTER_MASK);

	// Check if the counter has reset to 0
	EXPECT_EQ(counter2, 0);
}

// Test counter increment within same millisecond
TEST(UuidBuilderTest, CounterIncrementWithinTimestampTick) {
	auto builder = UuidBuilder::getTestBuilder();
	auto uuid1 = builder.build();

	// Extract the counter value from the UUID
	uint16_t counter1 = (uuid1.msb() & UUID_COUNTER_MASK);

	// Generate another UUID within the same timestamp tick
	auto uuid2 = builder.build();

	// Extract the counter value from the new UUID
	uint16_t counter2 = (uuid2.msb() & UUID_COUNTER_MASK);

	// Check if the counter has incremented by 1
	EXPECT_EQ(counter2, counter1 + 1);
}

// Test counter freeze at maximum value
// Custom timesource to avoid flakyness
TEST(UuidBuilderTest, CounterFreezeAtMaxValue) {
	auto builder = UuidBuilder::getTestBuilder().withTimeSource([]() {
		// Fixed timestamp to ensure the counter does not reset
		return std::chrono::system_clock::time_point(
		    std::chrono::milliseconds(1234567890123));
	});

	// Generate UUIDs until the counter reaches its maximum value
	for (int i = 0; i < 4095; ++i) {
		builder.build();
	}

	// Generate one more UUID when the counter is at its maximum value
	auto uuid = builder.build();

	// Extract the counter value from the UUID
	uint16_t counter = (uuid.msb() & UUID_COUNTER_MASK);

	// Check if the counter is frozen at its maximum value
	EXPECT_EQ(counter, 4095);
}

// Test counter max value
TEST_F(TestUuidBuilder, CounterMaxValue) {
	auto builder = UuidBuilder::getTestBuilder()
	                   .withTimeSource([]() {
		                   return std::chrono::system_clock::time_point(
		                       std::chrono::milliseconds(1234567890123));
	                   })
	                   .withRandomSource([]() { return 0x1234567890ABCDEF; });

	for (int i = 0; i < 4096; ++i) {
		builder.build();
	}
	auto uuid = builder.build();

	EXPECT_EQ(uuid.msb() & UUID_COUNTER_MASK, 4095);  // Counter max value
}

// Test custom time and random source with builder
TEST(UuidBuilderTest, CustomTimeAndRandomSource) {
	// Create a custom time source that returns a fixed timestamp
	auto fixed_time = std::chrono::system_clock::from_time_t(1623456789);
	auto time_source = [fixed_time]() { return fixed_time; };

	// Create a custom random source that returns a fixed random value
	uint64_t fixed_random = 0x1234567890ABCDEF;
	auto random_source = [fixed_random]() { return fixed_random; };

	// Create a UuidBuilder with the custom time and random sources
	auto builder = UuidBuilder::getTestBuilder()
	                   .withTimeSource(time_source)
	                   .withRandomSource(random_source);

	// Generate a UUID using the custom sources
	auto uuid = builder.build();

	// Extract the timestamp and random value from the generated UUID
	uint64_t timestamp = uuid.msb() >> UUID_TIMESTAMP_SHIFT;
	uint64_t random_value = uuid.lsb() & UUID_RANDOM_MASK;

	// Convert the fixed timestamp to milliseconds
	auto fixed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	                         fixed_time.time_since_epoch())
	                         .count();

	// Check if the generated UUID matches the expected values
	EXPECT_EQ(timestamp, fixed_time_ms);
	EXPECT_EQ(random_value, fixed_random);
}

}  // namespace
