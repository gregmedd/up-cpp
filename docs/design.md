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

High level:
```mermaid
classDiagram
    class RpcClient {
        - transport_
        - request_builder_
        + RpcClient(transport, method, priority, ttl, ...)
        + invokeMethod(payload)
        + invokeMethod()
    }

    class RpcServer {
        - transport_
        - ttl_
        - callback_
        - callback_handle_
        # RpcServer(transport, method)
        # connect(callback)
        + create(transport, method, callback, ...)$
    }

    class Publisher {
        - transport_
        - publish_builder_
        + Publisher(transport, topic, format)
        + publish(payload)
    }

    class Subscriber {
        - transport_
        - subscription_
        # Subscriber(transport, topic, listener)
        + subscribe(transport, topic, callback)$
    }

    class NotificationSource {
        - transport_
        - notify_builder_
        + NotificationSource(transport, source, sink, ...)
        + notify(payload)
    }

    class NotificationSink {
        - transport_
        - listener_
        # NotificationSink(transport, listener)
        + create(transport, sink, callback...)$
    }

    <<up_cpp_Layer2>> RpcClient
    <<up_cpp_Layer2>> RpcServer
    <<up_cpp_Layer2>> Publisher
    <<up_cpp_Layer2>> Subscriber
    <<up_cpp_Layer2>> NotificationSource
    <<up_cpp_Layer2>> NotificationSink

    class shared_ptr~UTransport~ {
    }

    <<stdlib>> shared_ptr~UTransport~

    RpcClient *-- shared_ptr~UTransport~
    RpcServer *-- shared_ptr~UTransport~
    Publisher *-- shared_ptr~UTransport~
    Subscriber *-- shared_ptr~UTransport~
    NotificationSource *-- shared_ptr~UTransport~
    NotificationSink *-- shared_ptr~UTransport~

    class UTransport {
        + send(message) UStatus
        + registerListener(sink_filter, callback, ...) Expected~Handle,UStatus~
        + getDefaultSource() UUri
        # sendImpl(message)* UStatus
        # registerListenerImpl(sink_filter, callable_listener, ...)* UStatus
        # cleanupListener(callable_listener)*
    }

    <<up_cpp_Layer1>> UTransport

    shared_ptr~UTransport~ o-- UTransport

    class ZenohUTransport {
        # sendImpl(message) UStatus
        # registerListenerImpl(sink_filter, callable_listener, ...) UStatus
    }
    <<up_transport_zenoh_cpp>> ZenohUTransport

    class VSomeIpUTransport {
        # sendImpl(message) UStatus
        # registerListenerImpl(sink_filter, callable_listener, ...) UStatus
    }
    <<up_transport_vsomeip_cpp>> VSomeIpUTransport

    UTransport <|.. ZenohUTransport : Implements
    UTransport <|.. VSomeIpUTransport : Implements
```

uE using up-cpp (and a transport)
```mermaid
erDiagram
    uEntity ||--|{ "up-cpp L2 Comm" : uses

    "up-cpp L2 Comm" }|--|| UTransport : uses

    UTransport ||..|| ZenohUTransport : "implemented by"
    UTransport ||..|| VSomeIpUTransport : "implemented by"

    uEntity ||--|| ZenohUTransport : "gets implementation"
    uEntity ||--|| VSomeIpUTransport : "gets implementation"
```
