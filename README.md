# keylogger (macOS)

Educational macOS keylogger + collector server, written in C++. For learning event-tap APIs, socket IO, and defensive research — **run only on machines you own or have explicit written authorization to instrument.**

## Files

- `keylogger_mac.cpp` — the client. Uses `CGEventTap` to capture key events, translates them via the current keyboard layout, buffers, and ships them to the server over TCP.
- `server_mac.cpp` — the collector. Listens on TCP `4444` and appends everything it receives to `captured_keys.log`.

## Build

```
clang++ -std=c++17 -framework ApplicationServices -framework Carbon -framework CoreFoundation keylogger_mac.cpp -o keylogger
clang++ -std=c++17 server_mac.cpp -o server
```

## Run

Edit `SERVER_IP` in `keylogger_mac.cpp` before building. Start the server first, then run the client. The client requires macOS Accessibility permission to receive events.

## Legal

Deploying a keylogger on a system you do not own or have not been authorized to instrument is illegal in most jurisdictions. Use for personal learning on your own hardware only.
