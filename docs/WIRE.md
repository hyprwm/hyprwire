# Wire Docs

The hyprwire format is split into _messages_. Each message is always in the form of:
```
[code : 1 byte][content : ...][end : 1 byte]
```

Content can be any length, and that needs to be verified by the parser. Each content message contains
variables, defined by a magic which defines what follows.

### Message codes

The message codes are documented alongside their parameters in [src/core/message/MessageType.hpp](../src/core/message/MessageType.hpp).

### Message content

Each code has a hard-defined content, as can be seen in the MessageType header. For messages with a `...` at
the end, the message should be read until an `END` is received in place where the next argument would be.

### Wire content types

Content is split into parameters, which are essentially variables. These are defined in [include/hyprwire/core/types/MessageMagic.hpp](../include/hyprwire/core/types/MessageMagic.hpp).

### Example wire message

An example, using `HW_MESSAGE_TYPE_SUP`, which takes a str:

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

## Estabilishing a connection

Estabilishing a connection is initiated by the client. The client must send `SUP` to the server with a single
parameter, a string of `"VAX"`. That's because I am a selfish asshole.

The server must respond with `HANDSHAKE_BEGIN`, with an array of `uint`s describing versions of the protocol
it supports. Currently, that's just `[1]`.

The client must send `HANDSHAKE_ACK` with the chosen version, that is `1`.

The server must reply with `HANDSHAKE_PROTOCOLS`. This contains an array of strings with supported
protocols. For example, if the server supports `my_protocol` at revision 2, and `my_other_protocol` at revision 1,
it should reply with `["my_protocol@2", "my_other_protocol@1"]`. As you can see, the revision is appended
with an `@[ver]`.

Once this is sent, the handshake is considered complete.

## Once connection is alive

Once the connection is alive, we can proceed with regular communication. In order to bind to a protocol exposed,
you must send a `BIND_PROTOCOL` with a `sequence` and a protocol spec string, e.g. `my_protocol@2`.

> ![NOTE]
> A sequence is a u32 that must not repeat. Keep a sequence counter, and every time you need a seq, send
> last_seq + 1.

Once the bind is successful on the server, the server will respond with `NEW_OBJECT`. This will contain
an object handle id and the client-provided sequence. This object handle id can be used to interact
with the protocol manager object (which is the first object in the protocol spec).

### Generic messages

Generic messages are described by protocol XMLs. These should send `u32` with the object ID from `NEW_OBJECT`,
a method ID, and then the arguments as defined in the XML.

The method ID is an identifier. These IDs follow the XML ordering, and are per-side, which means `s2c` methods
and `c2s` methods have independent counters. For example:

```
s2c: A
c2s: B
s2c: C
```

method A = ID 0, method B = ID 0, method C = ID 1.

### Fatal errors

If a client commits a fatal protocol error, a `FATAL_PROTOCOL_ERROR` message is sent and the connection
is terminated. That contains a `u32` with the object handle id that the error was committed on (or 0 if it's
not related to an object), a `u32` with the error id (or -1 if it's not an enum'd error) and a `varchar` with
the error message.

### Roundtrips

If the client wants to synchronize state, e.g. wait for the server to process requests it's sent to it,
it can request a roundtrip. `ROUNDTRIP_REQUEST` is sent with a sequence from the client, and the server must
respond with `ROUNDTRIP_DONE` once it receives it and processes all pending events that came before receiving it.

### Creating objects

Some `GENERIC_PROTOCOL_MESSAGE`s might create objects. In the protocol XMLs, that's done with `<returns iface="my_object_v1">`. In those cases, the first argument from the "data" part of the message
(everything after object + method) should be a `seq` which will be used to assign an ID to the new object
with a `NEW_OBJECT` event from the server. After the seq, regular message arguments should be passed.

Object IDs have no requirements. The server may choose them however it wants, as long as two objects that
are alive do not share an ID. The hyprwire reference server does a simple increment.

### Destroying objects

If a method is a destructor in the XML, calling the method must destroy the object on the server, and
any subsequent calls to the ID must raise a protocol error, unless the ID has already been reassigned to a new
object.

### Sample XML protocol spec

See [protocol-v1.xml](../tests/protocol-v1.xml)
