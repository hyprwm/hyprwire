# Wire Docs

This document describes the hyprwire wire format: what bytes go over the socket, and the rules
both sides must follow.

## Transport

Hyprwire runs over a Unix domain socket of type `SOCK_STREAM`. The client either connects to a socket
path, or is handed an already connected fd (e.g. one end of a `socketpair()`).

A few things follow from this:
- The stream is a sequence of messages sent back-to-back. Messages have no length prefix,
  so a parser has to parse a message to know where the next one begins. A single read may
  contain many messages.
- All fixed-width values are in host byte order, as both ends always live on the same machine.
  The examples in this document assume a little-endian host.
- File descriptors are passed as `SCM_RIGHTS` ancillary data. See [File descriptors](#file-descriptors).

## Messages

The hyprwire format is split into _messages_. Each message is always in the form of:
```
[code : 1 byte][content : ...][end : 1 byte]
```

The content is a list of arguments. Each argument starts with a 1 byte magic, which defines its type
and what follows. The message ends when an `END` (`0x00`) magic is found in place of the next argument.

Content can be any length, and that needs to be verified by the parser.

### Argument types

The magics are defined in [include/hyprwire/core/types/MessageMagic.hpp](../include/hyprwire/core/types/MessageMagic.hpp).

| Magic  | Name        | Layout                                   | Notes                                              |
|--------|-------------|------------------------------------------|----------------------------------------------------|
| `0x00` | `END`       | `[0x00]`                                 | Ends a message                                     |
| `0x10` | `UINT`      | `[0x10][u32]`                            |                                                    |
| `0x11` | `INT`       | `[0x11][i32]`                            |                                                    |
| `0x12` | `F32`       | `[0x12][f32]`                            | IEEE-754 single precision                          |
| `0x13` | `SEQ`       | `[0x13][u32]`                            | A sequence, see [Sequences](#sequences)            |
| `0x14` | `OBJECT_ID` | -                                        | Reserved, not implemented. Must not be sent        |
| `0x20` | `VARCHAR`   | `[0x20][len : VLQ][data : len bytes]`    | A string, not NUL-terminated                       |
| `0x21` | `ARRAY`     | `[0x21][type : 1 byte][count : VLQ][elements ...]` | See [Arrays](#arrays)                    |
| `0x22` | `OBJECT`    | `[0x22][u32]`                            | An object ID                                       |
| `0x40` | `FD`        | `[0x40]`                                 | The fd itself is sent as ancillary data            |

`VARCHAR`s used as protocol method arguments (including array elements) must not contain NUL bytes.
Empty strings are allowed.

### Variable length integers

Lengths and element counts are encoded as a VLQ (variable length quantity), split into groups of 7 bits,
**least significant group first** (like unsigned LEB128). The top bit of each byte is set if
another byte follows.

A VLQ is at most 4 bytes long, so the largest encodable value is `2^28 - 1`.

| Value | Encoded     |
|-------|-------------|
| 0     | `0x00`      |
| 3     | `0x03`      |
| 127   | `0x7F`      |
| 128   | `0x80 0x01` |
| 300   | `0xAC 0x02` |

### Arrays

An array carries the element type once, followed by the element count and the elements. The elements
do not have their own magic:
- `UINT`, `INT`, `F32`, `SEQ` and `OBJECT` elements are 4 bytes each.
- `VARCHAR` elements are `[len : VLQ][data : len bytes]`.
- `FD` elements have no inline data at all. `count` fds are sent as ancillary data.

Nested arrays are not allowed. An array can have at most 65535 elements.

For example, an array of two `UINT`s, `[1, 2]`:
```
0x21 0x10 0x02 0x01 0x00 0x00 0x00 0x02 0x00 0x00 0x00
```

### Message codes

The message codes are defined in [src/core/message/MessageType.hpp](../src/core/message/MessageType.hpp).

| Code   | Name                       | Direction | Arguments                                                  |
|--------|----------------------------|-----------|------------------------------------------------------------|
| `0x01` | `SUP`                      | C → S     | `VARCHAR`, must be `"VAX"`                                 |
| `0x02` | `HANDSHAKE_BEGIN`          | S → C     | `ARRAY(UINT)` supported wire versions                      |
| `0x03` | `HANDSHAKE_ACK`            | C → S     | `UINT` chosen wire version                                 |
| `0x04` | `HANDSHAKE_PROTOCOLS`      | S → C     | `ARRAY(VARCHAR)` exposed protocols                         |
| `0x0A` | `BIND_PROTOCOL`            | C → S     | `UINT` seq, `VARCHAR` protocol name, `UINT` version        |
| `0x0B` | `NEW_OBJECT`               | S → C     | `UINT` object ID, `UINT` seq                               |
| `0x0C` | `FATAL_PROTOCOL_ERROR`     | S → C     | `UINT` object ID, `UINT` error ID, `VARCHAR` error message |
| `0x0D` | `ROUNDTRIP_REQUEST`        | C → S     | `UINT` seq                                                 |
| `0x0E` | `ROUNDTRIP_DONE`           | S → C     | `UINT` seq                                                 |
| `0x64` | `GENERIC_PROTOCOL_MESSAGE` | both      | `OBJECT` object ID, `UINT` method ID, `...`                |

Each code has a hard-defined content. For messages with a `...` at the end, the message should be read until
an `END` is received in place where the next argument would be.

Receiving a message in the wrong direction (e.g. the client sending `NEW_OBJECT`) is a fatal protocol error.

### Example wire message

An example, using `SUP`, which takes a str:

```
0x01 0x20 0x03 0x56 0x41 0x58 0x00
 |    |    |     |   |    |    |
SUP   |  3 Bytes |   |    |    |
   VARCHAR       V   A    X   END
```

If a message does not end with an `END`, or a message code is encountered that is not valid,
or a content type is invalid, all those must result in a fatal protocol error.

As you can see, after the `SUP` code, we just read arguments until an `END`. If instead of the `END`,
we'd see e.g. `0x10` which means `UINT`, we would read 4 more bytes and repeat until we reach `END`.
In the above example, that would be a protocol error, as `SUP` expects a string of length 3 and nothing
more, but other messages might accept `n` arguments.

## Establishing a connection

Establishing a connection is initiated by the client. The client must send `SUP` to the server with a single
parameter, a string of `"VAX"`. That's because I am a selfish asshole.

The server must respond with `HANDSHAKE_BEGIN`, with an array of `uint`s describing versions of the wire protocol
it supports. Currently, that's just `[1]`.

The client must send `HANDSHAKE_ACK` with the chosen version, that is `1`. If the client supports none of
the offered versions, it must disconnect.

The server must reply with `HANDSHAKE_PROTOCOLS`. This contains an array of strings with the protocols
exposed to this client. For example, if the server supports `my_protocol` at revision 2, and `my_other_protocol` at revision 1,
it should reply with `["my_protocol@2", "my_other_protocol@1"]`. As you can see, the revision is appended
with an `@[ver]`. The name is everything before the last `@`.

The server may expose a different set of protocols to each client. Protocols that are not listed
must not be bound.

Once this is received by the client, the handshake is considered complete. The reference client gives up
if the handshake does not complete within 5 seconds.

### Example handshake

A full exchange, followed by a bind of `my_protocol` at version 1:

```
C → S  SUP                  01 20 03 "VAX" 00
S → C  HANDSHAKE_BEGIN      02 21 10 01 [01 00 00 00] 00
C → S  HANDSHAKE_ACK        03 10 [01 00 00 00] 00
S → C  HANDSHAKE_PROTOCOLS  04 21 20 01 0D "my_protocol@1" 00
C → S  BIND_PROTOCOL        0A 10 [01 00 00 00] 20 0B "my_protocol" 10 [01 00 00 00] 00
S → C  NEW_OBJECT           0B 10 [01 00 00 00] 10 [01 00 00 00] 00
```

Strings are shown in quotes for readability, and `[...]` groups the bytes of a single `u32`.

## Once connection is alive

Once the connection is alive, we can proceed with regular communication. In order to bind to a protocol exposed,
you must send a `BIND_PROTOCOL` with:
- a `seq`
- the protocol name, without the revision, e.g. `my_protocol`
- the version to bind. This must be at least `1`, and not higher than the revision the server advertised.

Once the bind is successful on the server, the server will respond with `NEW_OBJECT`. This will contain
an object handle ID and the client-provided seq. This object handle ID can be used to interact
with the protocol manager object (which is the first object in the protocol spec). The object has the
version that was requested in the bind.

Binding a protocol that was not exposed, or at a version higher than the server supports, is a fatal protocol error.

A client may send multiple binds without waiting for the replies, and match the `NEW_OBJECT`s by their seq.

### Sequences

A sequence is a u32 picked by the client to match a reply from the server with its request.

> [!NOTE]
> A sequence must not repeat. Keep a sequence counter, and every time you need a seq, send
> last_seq + 1. Start at `1`, as `0` is never a valid seq.

Binds and object creation (see [Creating objects](#creating-objects)) share a single counter.
Roundtrips use their own one.

### Object IDs

Object IDs are assigned by the server. They have no other requirements: the server may choose them however it wants,
as long as two objects that are alive do not share an ID. The hyprwire reference server does a simple increment,
starting at `1`.

`0` is never a valid object ID, and means "no object" where an object ID is optional, e.g. in `FATAL_PROTOCOL_ERROR`.
A `NEW_OBJECT` with an ID of `0` is a protocol error.

### Generic messages

Generic messages are described by protocol XMLs. These are `GENERIC_PROTOCOL_MESSAGE`s which contain
an `OBJECT` with the object ID from `NEW_OBJECT`, a `UINT` method ID, and then the arguments as defined in the XML:

```
[0x64][0x22][object ID : u32][0x10][method ID : u32][arguments ...][0x00]
```

For example, calling method `0` with the string `"hi"` on object `1`:

```
0x64 0x22 0x01 0x00 0x00 0x00 0x10 0x00 0x00 0x00 0x00 0x20 0x02 0x68 0x69 0x00
```

The method ID is an identifier. These IDs follow the XML ordering, and are per-side, which means `s2c` methods
and `c2s` methods have independent counters. For example:

```
s2c: A
c2s: B
s2c: C
```

method A = ID 0, method B = ID 0, method C = ID 1.

The arguments must match the XML exactly. Each of these is a fatal protocol error:
- a message for an object ID that does not exist
- a method ID that does not exist for that object, in that direction
- calling a method with a `since` higher than the object's version
- an argument of a different type than in the XML, including the element type of arrays
- missing or extra arguments

Enums are sent as `UINT`s.

### Fatal errors

If a client commits a fatal protocol error, a `FATAL_PROTOCOL_ERROR` message is sent and the connection
is terminated. That contains a `u32` with the object handle ID that the error was committed on (or 0 if it's
not related to an object), a `u32` with the error ID (or `0xFFFFFFFF` if it's not an enum'd error) and a `varchar` with
the error message.

Only the server sends `FATAL_PROTOCOL_ERROR`. If the server commits a protocol error, the client simply disconnects.

### Roundtrips

If the client wants to synchronize state, e.g. wait for the server to process requests it's sent to it,
it can request a roundtrip. `ROUNDTRIP_REQUEST` is sent with a sequence from the client, and the server must
respond with `ROUNDTRIP_DONE` once it receives it and processes all pending messages that came before receiving it.

Roundtrip sequences must be non-zero and increasing. The server may answer only the latest of multiple pending
requests, so a `ROUNDTRIP_DONE` for seq `n` acknowledges all requests up to and including `n`.

### Creating objects

Some `GENERIC_PROTOCOL_MESSAGE`s might create objects. In the protocol XMLs, that's done with `<returns iface="my_object_v1">`.
In those cases, the first argument from the "data" part of the message (everything after object + method) must be a `SEQ`
(`0x13`, not `UINT`), which will be used to assign an ID to the new object with a `NEW_OBJECT` event from the server.
After the seq, regular message arguments should be passed.

For example, calling `make_object` (c2s method `5` of `my_manager_v1` in the [sample protocol](../tests/protocol-v1.xml)),
which has no other arguments, on object `1` with seq `2`:

```
0x64 0x22 0x01 0x00 0x00 0x00 0x10 0x05 0x00 0x00 0x00 0x13 0x02 0x00 0x00 0x00 0x00
```

The new object belongs to the same protocol, and has the same version, as the object the method was called on.

Only the client can call methods that create objects. Until the client receives the `NEW_OBJECT`, it does not
know the ID of the new object, and must not send any messages to it. The reference client queues such messages,
and sends them once the ID is known.

### Destroying objects

If a method is a destructor in the XML, calling the method must destroy the object on both sides, and
any subsequent calls to the ID must raise a protocol error, unless the ID has already been reassigned to a new
object. There is no acknowledgement. Destructors can go in either direction.

When a client disconnects, all of its objects are destroyed.

### File descriptors

Each `FD` argument is just the `0x40` magic in the message. The fds themselves are sent as `SCM_RIGHTS` ancillary data
in the same `sendmsg()` call as the message. The receiver assigns the received fds to the `FD` arguments
(and `FD` array elements) in the order in which they appear in the stream.

A message can carry at most 255 fds. The kernel may impose a lower limit (e.g. 253 on Linux).

Having fewer fds than `FD` arguments, or receiving fds that are not used by any argument, is a fatal protocol error.

## Limits

| What                                    | Limit                 |
|-----------------------------------------|-----------------------|
| VLQ length                              | 4 bytes (`2^28 - 1`)  |
| Array elements                          | 65535                 |
| Versions in `HANDSHAKE_BEGIN`           | 255                   |
| Protocols in `HANDSHAKE_PROTOCOLS`      | 2047                  |
| Fds per message                         | 255                   |

## Sample XML protocol spec

See [protocol-v1.xml](../tests/protocol-v1.xml)
