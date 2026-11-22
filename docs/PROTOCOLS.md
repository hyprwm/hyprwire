# Protocol Docs

Hyprwire protocols are described in XML files. `hyprwire-scanner` turns those into C++ code for the
server or client side. For what goes over the wire, see [WIRE.md](./WIRE.md).

See [protocol-v1.xml](../tests/protocol-v1.xml) for a complete example.

## Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<protocol name="my_protocol_v1" version="1">
  <copyright>
    Copyright notice, copied into the generated files
  </copyright>

  <object name="my_manager_v1" version="1">
    <description summary="manager object">
      The first object is created when the client binds the protocol.
    </description>

    <c2s name="make_object">
      <description summary="Make an object"/>
      <returns iface="my_object_v1"/>
    </c2s>
  </object>

  <enum name="my_enum">
    <value idx="0" name="hello"/>
    <value idx="4" name="world"/>
  </enum>

  <object name="my_object_v1" version="1">
    <s2c name="send_message">
      <arg name="message" type="varchar" summary="a message"/>
    </s2c>

    <c2s name="send_enum" since="1">
      <arg name="value" type="enum" interface="my_enum" summary="an enum"/>
    </c2s>

    <c2s name="destroy" destructor="true"/>
  </object>
</protocol>
```

## Elements

### `<protocol>`

The root element.

| Attribute | Description                                                                              |
|-----------|------------------------------------------------------------------------------------------|
| `name`    | The protocol name. This is what's advertised in the handshake and used to bind           |
| `version` | The protocol revision, defaults to `1`. Bump it when adding methods, see `since` below    |

### `<copyright>`

Optional. The contents are copied into a comment in the generated files.

### `<object>`

An object (interface) of the protocol.

The **first** `<object>` in the file is special: it's the object created when a client binds the protocol,
usually a manager. All other objects can only be created by methods with a `<returns>`.

| Attribute | Description                                  |
|-----------|----------------------------------------------|
| `name`    | The object name                              |
| `version` | The object version. Currently informational  |

Every object has the version that the client bound the protocol at. Objects created with `<returns>`
inherit the version of the object they were created from.

### `<c2s>` and `<s2c>`

A method, sent from the client to the server (`c2s`), or from the server to the client (`s2c`).

Method IDs are assigned in the order the methods appear in the object, with separate counters for `c2s`
and `s2c`. Reordering or inserting methods breaks compatibility, so always add new methods at the end.

| Attribute    | Description                                                                                 |
|--------------|---------------------------------------------------------------------------------------------|
| `name`       | The method name                                                                             |
| `since`      | Optional. The object version the method was added in. Calling it on an object with a lower version is a protocol error |
| `destructor` | Optional. If `true`, calling the method destroys the object on both sides                   |

A method contains any number of `<arg>`s, and optionally one `<returns>`.

### `<arg>`

An argument of a method. Arguments are sent in the order they are defined.

| Attribute   | Description                                     |
|-------------|-------------------------------------------------|
| `name`      | The argument name                               |
| `type`      | The argument type, see below                    |
| `interface` | For `enum`s, the name of the `<enum>`           |
| `summary`   | Optional. A description                         |

| Type           | Sent as                       | C++ type                                   |
|----------------|-------------------------------|--------------------------------------------|
| `uint`         | `UINT`                        | `uint32_t`                                 |
| `int`          | `INT`                         | `int32_t`                                  |
| `f32`          | `F32`                         | `float`                                    |
| `varchar`      | `VARCHAR`                     | `const char*`                              |
| `fd`           | `FD`                          | `int`                                      |
| `enum`         | `UINT`                        | the generated enum type                    |
| `array <type>` | `ARRAY` of `type`             | `const std::vector<T>&`                    |

Array element types can be `uint`, `int`, `f32`, `varchar` or `fd`. Objects cannot be passed as arguments yet.

### `<returns>`

Makes the method create a new object. Only allowed in `<c2s>` methods.

| Attribute | Description                            |
|-----------|----------------------------------------|
| `iface`   | The name of the `<object>` to create   |

### `<enum>`

An enum. Enums are protocol-wide, and can be used in `enum` arguments, or as error codes.

| Attribute | Description   |
|-----------|---------------|
| `name`    | The enum name |

It contains `<value>`s:

| Attribute     | Description                 |
|---------------|-----------------------------|
| `idx`         | The numeric value           |
| `name`        | The value name              |
| `description` | Optional. A description     |

### `<description>`

Optional, on objects and methods. Documentation only, ignored by the scanner.

## Generated code

`hyprwire-scanner` generates a `<protocol name>-spec.hpp` header, and either `<protocol name>-server.{hpp,cpp}`,
or `<protocol name>-client.{hpp,cpp}` with `--client`.

Names are derived from the XML names. For a protocol named `my_protocol_v1`:

| XML                                | Server                 | Client                 |
|------------------------------------|------------------------|------------------------|
| `<protocol name="my_protocol_v1">` | `CMyProtocolV1Impl`    | `CCMyProtocolV1Impl`   |
| `<object name="my_object_v1">`     | `CMyObjectV1Object`    | `CCMyObjectV1Object`   |
| `<c2s name="send_enum">`           | `setSendEnum(fn)`      | `sendSendEnum(...)`    |
| `<s2c name="send_message">`        | `sendSendMessage(...)` | `setSendMessage(fn)`   |

Enums are generated in the spec header, e.g. `<enum name="my_enum">` becomes `enum myProtocolV1MyEnum : uint32_t`,
with values like `MY_PROTOCOL_V1_MY_ENUM_HELLO`.

For methods with a `<returns>`:
- on the client, `sendMakeObject()` returns the new object, which can be wrapped in the generated class.
- on the server, the handler receives the seq as its first argument, and should create the object with
  `IServerSocket::createObject(client, parentObject, "my_object_v1", seq)`.

Server objects can raise a protocol error with `error(code, message)`, where `code` is usually a value of an `<enum>`.
