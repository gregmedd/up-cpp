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

## Interaction model for uEntities

A uEntity is an application or system component that communicates over
uProtocol. These uE may be composed of multiple modules, each of which could
operate on the interfaces of up-cpp (typically at _communication Layer 2_).

The uE is responsible for providing a `std::shared_ptr<UTransport>` to each
_Layer 2_ object instantiated, potentially through some platform abstraction
provided by the uE's platform ecosystem.

```mermaid
erDiagram
    uEntity ||--|{ "uEntity Modules" : contains
    "uEntity Modules" ||--|{ "up-cpp L2 Comm" : uses

    "up-cpp L2 Comm" }|--|| "up-cpp L1 Xport" : uses

    "up-cpp L1 Xport" ||..|| ZenohUTransport : "implemented by"
    "up-cpp L1 Xport" ||..|| VSomeIpUTransport : ""

    uEntity ||--|| "uEntity Platform Abstraction": uses

    "uEntity Platform Abstraction" ||--|| ZenohUTransport : ""
    "uEntity Platform Abstraction" ||--|| VSomeIpUTransport : instantiates
```

up-cpp is responsible for providing **something about validation and protocol
compliance, with the best results from using L2**.

## Example interactions

### Publisher / Subscriber

```mermaid
sequenceDiagram
    box uEntity A
    participant uE Main
    participant uE Subscriber
    end

    box up-cpp
    participant Subscriber
    participant UTransport
    end

    box up-transport-example-cpp
    participant ExampleUTransport
    end

    box up-cpp (B)
    participant UTransport (B)
    participant Publisher
    end

    box uEntity (B)
    participant uE Publisher
    end

    autonumber

    uE Main->>+ExampleUTransport: make_shared<FooUTransport>(config)
    note right of uE Main: Could be instantiated using<br/>a platform abstraction<br/>provided by the uE's<br/>environment
    ExampleUTransport->>-uE Main: give shared_ptr<FooUTransport>
    uE Main->>+uE Subscriber: give shared_ptr<UTransport>
    uE Subscriber->>+Subscriber: subscribe(transport, topic, callback)
    Subscriber->>+UTransport: registerListener(topic, callback)
    UTransport->>-Subscriber: ListenHandle OR UStatus
    Subscriber->>-uE Subscriber: unique_ptr<Subscriber> OR UStatus
    uE Subscriber->>-uE Main: 

    uE Publisher-->uE Publisher: Running

    uE Publisher->>+Publisher: publish(payload)
    Publisher->>Publisher: [Build Message]
    Publisher->>+UTransport (B): send(message)
    UTransport (B)->>+ExampleUTransport: sendImpl(message)
    par Result of send
        ExampleUTransport->>-UTransport (B): give UStatus
        UTransport (B)->>-Publisher: give UStatus
        Publisher->>-uE Publisher: give UStatus
    and Send over transport
        activate ExampleUTransport
        ExampleUTransport->>+uE Subscriber: callback(message)
        deactivate uE Subscriber
        deactivate ExampleUTransport
    end
```

### RPC Client / Server

### Error handling and propagation

See [Error propagation model for up-cpp and related libraries](./error_propagation.md).

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

## Threading and synchronization model of up-cpp

**TODO: Any L1 or L2 interface can and will be called by multiple threads. They
must be thread safe**

**TODO: Should the base UTransport have a thread pool for servicing callbacks?
It doesn't seem like a good idea to rely on the transport implementations to
handle that. Most likely, one thread pool per UTransport is adequate to handle
the callbacks.**

**TODO: Any standard container will need to be mutex guarded. Recommend a
shared_lock for read-heavy containers. Recommend SafeMap model for associative
containers**

**TODO: All registered callbacks need to be thread safe. They could be invoked
multiple times simultaneously from different execution contexts. Hmm... is this
really the reqirement we want to impose? Is there another way to serialize
the requests (possibly through the connection objects?) without hogging all
the pool resources?**

_Possibly defer some design to the individual communication modules, looking
only at their operation modes. Maybe UTransport shouldn't have any sort of
thread pool_

## Error handling and propagation

See [Error propagation model for up-cpp and related libraries](./error_propagation.md).
