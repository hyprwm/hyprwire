## hyprwire
A fast and consistent wire protocol for IPC

## What is hyprwire

Hyprwire is a fast and consistent wire protocol, and its implementation. This is essentially a
"method" for processes to talk to each other.

### How does hyprwire differ from other things?

Hyprwire is heavily inspired by Wayland, and heavily anti-inspired by D-Bus.

Hyprwire is:
- Strict: both sides need to be on the same page to communicate. No "random data" is allowed.
- Fast: initial handshakes are very simple and allow for quick information exchange (including one-shot operations)
- Simple to use: the API uses modern C++ and abstracts away any memory-sensitive operations
- Simple internally: the protocol itself is simple and straightforward to parse / write your own implementation

## Building

Dependencies:
- a C++23 compiler, cmake and pkg-config
- hyprutils >= 0.9.0
- libffi
- pugixml (for the scanner)
- gtest (for tests only)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

A `Debug` build also builds the tests, which can be run with `ctest --test-dir build`.

## Usage

Protocols are described in XML files, see [PROTOCOLS.md](./docs/PROTOCOLS.md). To generate code from one, use
`hyprwire-scanner`:

```sh
hyprwire-scanner my_protocol.xml out/           # server side
hyprwire-scanner --client my_protocol.xml out/  # client side
```

For a full example of a server and a client, see [tests/Server.cpp](./tests/Server.cpp) and
[tests/Client.cpp](./tests/Client.cpp).

### Debugging

Set `HW_TRACE=1` to log every message sent and received.

### Developer wire docs

See [WIRE.md](./docs/WIRE.md)