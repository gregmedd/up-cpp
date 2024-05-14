# Design notes for up-cpp

up-cpp provides a common interface for building uProtocol applications
(uEntities or uE) in C++.

Components:

* Datamodel operations (`up-cpp/datamodel/`)
  * Builders
  * Serializers
  * Validators
* L1 Transport (`up-cpp/transport/`)
* L2 Communications (`up-cpp/communications`)
* Common utilities (`up-cpp/utils/`)

For the vast majority of uE, the _L2 Communications_ modules will be the
primary point of interaction. Transport implementations (e.g.
[up-client-zenoh-cpp](/eclipse-uprotocol/up-client-zenoh-cpp)), on the other
hand, will be responsible for implementing the virtual interfaces found in
_L1 Transport_.

# Overview

```mermaid
erDiagram
    uE ||--|{ "uE Modules" : contains
    "uE Modules" ||..|{ RpcClient : contains
    "uE Modules" ||..|{ RpcServer : contains
    "uE Modules" ||..|{ Publisher : contains
    "uE Modules" ||..|{ Subscriber : contains
    "uE Modules" ||..|{ NotificationSource : contains
    "uE Modules" ||..|{ NotificationSink : contains

    RpcClient {
        public RpcClient
        public invokeMethod
    }

    RpcServer {
        public RpcServer
    }

    Publisher {
        public Publisher
        public publish
    }

    Subscriber {
        static subscribe
    }

    NotificationSource {
        public NotificationSource
        public notify
    }

    NotificationSink {
        static create(sink_and_callback)
    }

    RpcClient }|--|| UTransport : uses
    RpcServer }|--|| UTransport : uses
    Publisher }|--|| UTransport : uses
    Subscriber }|--|| UTransport : uses
    NotificationSource }|--|| UTransport : uses
    NotificationSink }|--|| UTransport : uses

    UTransport {
        public send
        public listen
        public getDefaultUri
        virtual send
        virtual listen
    }

    UTransport ||..|| ZenohUTransport : "implemented by"
    UTransport ||..|| SomeIpUTransport : "implemented by"

    ZenohUTransport {
        virtual send
        virtual listen
    }

    SomeIpUTransport {
        virtual send
        virtual listen
```
