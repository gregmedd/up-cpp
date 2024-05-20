# Design notes for up-cpp

up-cpp provides a common interface for building uProtocol applications
(uEntities or uE) in C++. This library is composed of layers, each one building
on the one before:

* Common utilities (`up-cpp/utils/`)
* Datamodel operations (`up-cpp/datamodel/`)
  * Builders (`up-cpp/datamodel/builder/`)
  * Serializers (`up-cpp/datamodel/serializer/`)
  * Validators (`up-cpp/datamodel/validator/`)
* L1 Transport (`up-cpp/transport/`)
* L2 Communications (`up-cpp/communications`)

Most uE will operate at _Layer 2_, which provides interfaces for common
communications models, such as pub/sub, notficiations, and RPC. This
communications layer is composed of calls to _Layer 1_ for transport operations
such as sending and listening for messages. Note: In some limited cases,
specialized uE may operate directly on the transport layer. uStreamer, for
example, would be one such entity.

_Layer 1_ transport is exposed as a single virtual `UTransport` interface,
allowing for transport-independent at layers above this point. The actual
transport implementations are provided as separate libraries (e.g.
[up-client-zenoh-cpp](/eclipse-uprotocol/up-client-zenoh-cpp)).

The _Datamodel_ layer (sometimes called "Layer 0") is made up of protobuf
message objects, defined by the [uProtocol Specification](/eclipse-uprotocol/up-spec),
and tools for creating, validating, and serializing these objects.

Finally, the _Utilities_ layer contains common objects and functions shared
across up-cpp and transport implementations. Examples include objects for
callback management (`CallbackConnection.h`), thread pools (`ThreadPool.h`),
and more (`base64.h`).

```mermaid
erDiagram
    "L1 Transport" ||..|{ "Transport Implementation" : "Implemented by"
    "Datamodel" }|--|{ "Utilities" : uses

    "L2 Communication" }|--|| "L1 Transport" : uses
    "L2 Communication" }|--|{ "Datamodel" : uses
    "L2 Communication" }|--|{ "Utilities" : uses
    "L1 Transport" ||--|{ "Utilities" : uses
    "L1 Transport" ||--|{ "Datamodel" : uses

    "Transport Implementation" }|--|{ "Utilities" : uses
```

## Classes and interfaces of up-cpp



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
