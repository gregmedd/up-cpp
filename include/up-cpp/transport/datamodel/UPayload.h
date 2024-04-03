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
 * 
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2024 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _UPAYLOAD_H_
#define _UPAYLOAD_H_

#include <memory>
#include <cstdint>
#include <cstring>

namespace uprotocol::utransport {

    enum class UPayloadType {
        VALUE = 0, /* data passed by value - will be copied */
        REFERENCE, /* data passed by reference - the user need to ensure that the reference is valid until data is sent*/
        SHARED,    /* data passed by shared pointer */
        UNDEFINED  /* invalid */
    };

    // The Serialization format for the data stored in UPayload.
    enum UPayloadFormat {
        UNSPECIFIED = 0,             /* Payload format was not is not set */
        PROTOBUF_WRAPPED_IN_ANY = 1, /* Payload is an Any protobuf message that contains the packed payload */
        PROTOBUF = 2,                /* Protobuf serialization format */
        JSON = 3,                    /* JSON serialization format */
        SOMEIP = 4,                  /* Basic SOME/IP serialization format */
        SOMEIP_TLV = 5,              /* SOME/IP TLV format */
        RAW = 6,                     /* RAW (binary) format */
        TEXT = 7                     /* Text format */
    };          

    /**
    * The UPayload contains the clean Payload information at its raw serialized structure of a byte[]
    */
    class UPayload {

        public:
            // Constructor
            UPayload(const std::shared_ptr<const std::vector<uint8_t>> dataPtr, 
                     const UPayloadType type, const UPayloadFormat format) : type_(type), payloadFormat_(format) {

                if (type == UPayloadType::REFERENCE) {
                    dataPtr_ = dataPtr;
                } else {
                    dataPtr_ = std::make_shared<const std::vector<uint8_t>>(*dataPtr);
                }
            }

            // Copy constructor
            UPayload(const UPayload& other) : type_(other.type_), payloadFormat_(other.payloadFormat_) {
                if (type_ == UPayloadType::REFERENCE) {
                    dataPtr_ = other.dataPtr_;
                } else {
                    dataPtr_ = std::make_shared<const std::vector<uint8_t>>(*(other.dataPtr_));
                }
            }

            // Assignment operator
            UPayload& operator=(const UPayload& other) {
                if (this != &other) {
                    type_ = other.type_;
                    payloadFormat_ = other.payloadFormat_;
                    if (type_ == UPayloadType::REFERENCE) {
                        dataPtr_ = other.dataPtr_;
                    } else {
                        dataPtr_ = std::make_shared<const std::vector<uint8_t>>(*(other.dataPtr_));
                    }
                }
                return *this;
            }

            // Move constructor
            UPayload(UPayload&& other) noexcept 
                : dataPtr_(std::move(other.dataPtr_)), type_(other.type_), payloadFormat_(other.payloadFormat_) {
                    other.type_ = UPayloadType::UNDEFINED;
                    other.payloadFormat_ = UPayloadFormat::UNSPECIFIED;
                }

            // Move assignment operator
            UPayload& operator=(UPayload&& other) noexcept {
                if (this != &other) {
                    type_ = other.type_;
                    payloadFormat_ = other.payloadFormat_;
                    dataPtr_ = std::move(other.dataPtr_);
                    other.type_ = UPayloadType::UNDEFINED;
                    other.payloadFormat_ = UPayloadFormat::UNSPECIFIED;
                }
                return *this;
            }

            /**
            * @return data
            */
            std::shared_ptr<const std::vector<uint8_t>> data() const {
                return dataPtr_;
            }

            /**
            * @return size
            */
            size_t size() const {
                if (dataPtr_) {
                    return dataPtr_->size();
                }
                return 0;
            }

           /**
            * @return format type
            */
            UPayloadFormat format() const {
                return payloadFormat_;
            }
            
            bool isEmpty() const {
                if (dataPtr_) {
                    return dataPtr_->empty();
                }
                return true;
            }

        private:
            std::shared_ptr<const std::vector<uint8_t>> dataPtr_;
            UPayloadType type_ = UPayloadType::UNDEFINED;
            UPayloadFormat payloadFormat_ = UPayloadFormat::RAW;
    };
}

#endif /* _UPAYLOAD_H_ */
