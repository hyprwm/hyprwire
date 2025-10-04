## hyprwire
A fast and consistent wire protocol for IPC

> [!IMPORTANT]
> hyprwire is in a very early stage of development. It's not finished yet
> or stable.

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

