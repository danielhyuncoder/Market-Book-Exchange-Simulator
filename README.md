# Market Book Exchange Simulator

A three-process simulation of an exchange order-flow pipeline, built in C++20 with Boost.Asio: a **client** that submits orders over TCP, a **matching server** that validates, books, and matches those orders, and one or more **subscribers** that receive live order-book snapshots over UDP multicast.

```
 ┌────────┐     TCP (orders)      ┌────────────┐     UDP multicast     ┌──────────────┐
 │ Client │ ────────────────────> │   Server   │──────────────────────>│  Subscriber  │
 │        │<───────────────────── │ (matching  │                       │ (N instances)│
 └────────┘   TCP (ACK / ERROR)   │   engine)  │                       └──────────────┘
                                  └────────────┘
```

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Components](#components)
- [Order Flow](#order-flow)
- [Wire Protocol](#wire-protocol)
- [Configuring Message Size (18–30 bytes)](#configuring-message-size-1830-bytes)
- [JSON Configuration Files](#json-configuration-files)
- [Build](#build)
- [Run](#run)
- [Configuration Reference](#configuration-reference)
- [Error Codes](#error-codes)

---

## Overview

The simulator models the core mechanics of an electronic exchange:

- Clients submit **SEND**, **MODIFY**, and **KILL** order requests over a persistent TCP connection, using a compact, fixed-width binary wire format (not JSON — JSON is only used for local configuration/seed files, not on the wire).
- The server decodes and validates each request, assigns it a unique order ID, and hands it off to a lock-free queue for asynchronous processing — keeping the network I/O thread(s) decoupled from matching logic.
- Orders are matched per-symbol using **price-time priority**, with symbols distributed across multiple independent shards so unrelated symbols can be matched in parallel.
- After every book-changing event, the server serializes a depth-of-book snapshot and multicasts it over UDP, so any number of subscribers can listen in without adding load back onto the matching path.

## Project Structure

```
HFTExchangeSimulator/
├── vcpkg.json                       # dependency manifest (boost-asio, boost-system)
│
├── client/
│   ├── CMakeLists.txt
│   ├── client_config.h              # ports, IPs, wire-format byte widths
│   ├── data/orders.json             # orders replayed on startup
│   ├── include/
│   │   ├── client_classes.hpp       # StandardClient: connect, encode, send, listen for ACK/ERROR
│   │   ├── conversions.hpp          # byte <-> number codec, ACK/ERROR decoding
│   │   └── json.hpp                 # nlohmann::json (vendored)
│   └── src/main.cpp
│
├── server/
│   ├── CMakeLists.txt
│   ├── server_config.h              # ports, thread counts, byte widths, error codes
│   ├── data/symbols.json            # tradable symbols loaded at startup
│   ├── include/
│   │   ├── enums.hpp                # ORDER_TYPE, REQUEST_TYPE, ORDER, OB_SNAPSHOT
│   │   ├── conversions.hpp          # wire decode + snapshot serialization
│   │   ├── order_book.hpp           # ORDER_BOOK, sharding, matching engine
│   │   ├── server_classes.hpp/.cpp  # MatchingSession, MatchingEngine, OB_MCAST_FEED, ServerHandler
│   │   └── json.hpp
│   └── src/main.cpp
│
└── client_subscriber/
    ├── CMakeLists.txt
    ├── subscriber_config.h          # byte widths, multicast IP/port
    ├── include/
    │   ├── enums.hpp                 # CLIENT_ORDER, CLIENT_SNAPSHOT
    │   ├── conversions.hpp           # snapshot decode
    │   └── subscriber_classes.hpp    # Subscriber: joins multicast group, decode loop
    └── src/main.cpp
```

Each of `client/`, `server/`, and `client_subscriber/` is an independent CMake project with its own executable, so they build and run as three separate binaries.

## Components

### Client (`HFTExchangeSimulatorClient`)
- Opens a TCP connection to the server and binds to a fixed local port.
- Reads `data/orders.json` on startup and replays every entry in order as a **SEND**, **MODIFY**, or **KILL** message.
- Encodes each order into the fixed-width binary format described below and writes it to the socket.
- Runs an async listener alongside order submission that decodes incoming **ACK** and **ERROR** responses and prints them.

### Server (`HFTExchangeSimulator`)
- Loads the tradable symbol list from `data/symbols.json` at startup.
- Accepts TCP connections; each connected client gets its own `MatchingSession`, which reads, decodes, and lightly validates (price/quantity sanity checks) incoming order messages, replying with an ACK or a coded ERROR.
- Assigns each accepted order a monotonically increasing, server-issued order ID and pushes it onto a fixed-capacity **lock-free queue**, decoupling network I/O from book processing.
- A separate pool of worker thread(s) drains that queue and applies each order to the correct **order book**, which is chosen by hashing the symbol and routing it to one of several independent **shards** — so different symbols can be matched fully in parallel with no lock contention between them.
- Within a book, bids and asks are kept in sorted maps ordered by price (best bid / best ask always at the front), with time priority preserved via per-price-level FIFO queues. Matching runs price-time priority: whenever the best bid crosses the best ask, the smaller of the two quantities is filled and the matched side's remaining quantity (if any) stays at the front of the book.
- After a configurable number of book-changing events, the server builds a depth-of-book snapshot (top N price levels per side) and multicasts it as a UDP datagram.

### Subscriber (`ClientSubscriber`)
- Joins the configured UDP multicast group/port.
- Continuously receives and decodes snapshot datagrams into symbol, sequence number, and both sides of the book (price, quantity, order ID for each level).
- Optionally prints each decoded snapshot to stdout — useful for verifying the matching engine's output live, or as a starting point for building a market-data display on top.
- Multiple subscriber instances can run simultaneously against the same feed with no coordination needed, since UDP multicast fans out to every listener.

## Order Flow

```
Client                          Server                                          Subscribers
  │                                │
  │──TCP: SEND/MODIFY/KILL───────▶│  MatchingSession decodes + validates
  │                                │  → assigns order ID
  │◀──TCP: ACK / ERROR─────────────│  → pushes onto lock-free queue
  │                                │
                                   │  Worker thread pops queue
                                   │  → hash symbol → route to shard
                                   │  → apply SEND / MODIFY / KILL to book
                                   │  → run price-time-priority matching
                                   │  → every N events: build snapshot
                                   │──UDP multicast: snapshot──────────────────▶│  decode + display
```

- **SEND** places a new resting order into the book on the given side, at the given price and quantity.
- **MODIFY** is implemented as a kill of the existing order followed by a fresh send at the new price/quantity — meaning a modify loses its original time priority, consistent with how many real venues treat a price/quantity change.
- **KILL** cancels a resting order by its server-assigned ID. Cancellation is applied lazily: the ID is marked cancelled immediately, and the order is physically removed from the book the next time it's encountered at the front of its price level (either by direct match or by another operation touching that level).

## Wire Protocol

Client ↔ server communication uses a compact, fixed-width binary format rather than JSON — this keeps encode/decode cost minimal and message size fully predictable, which matters for a system modeling low-latency order entry.

**Request (client → server):**

| Offset | Field | Size | Notes |
|---|---|---|---|
| 0 | Request type | 1 byte | `'O'` = SEND, `'U'` = MODIFY, `'X'` = KILL |
| 1 | Side | 1 byte | `'B'` = buy, otherwise sell |
| 2 | Symbol | `SYMBOL_BYTES` | raw ASCII ticker |
| — | Quantity | `QTY_BYTES` | SEND / MODIFY |
| — | Price level | `PRICE_BYTES` | SEND / MODIFY, expressed in ticks |
| — | Order ID | `ID_BYTES` | MODIFY / KILL — the ID being acted on |

All multi-byte integer fields are encoded big-endian, at a fixed width driven entirely by configuration (see below) — there's no variable-length encoding or delimiters, so both sides just need to agree on the byte widths.

**Response (server → client):**

| Message | Format |
|---|---|
| ACK | `'A'` + acknowledgment of the processed order |
| ERROR | `'J'` + 4-byte error code |

**Snapshot (server → subscribers, UDP multicast):**

| Field | Size |
|---|---|
| Symbol | `SYMBOL_BYTES` |
| Sequence number | 8 bytes |
| Buy side | `SNAPSHOT_LEN` × (order ID + price + quantity) |
| Sell side | `SNAPSHOT_LEN` × (order ID + price + quantity) |

`SNAPSHOT_LEN` controls the book depth included per side (10 by default).

## Configuring Message Size (18–30 bytes)

The width of every numeric field in the request/response format — quantity, price, and order ID — is controlled by three macros defined identically in `server_config.h`, `client_config.h`, and `subscriber_config.h`:

```c
#define SYMBOL_BYTES 4
#define QTY_BYTES    8   // 8 = long long range, 4 = int range
#define PRICE_BYTES  8
#define ID_BYTES     8
```

The full request frame size is always:

```
2 (header) + SYMBOL_BYTES + QTY_BYTES + PRICE_BYTES + ID_BYTES
```

Because the codec (`number_to_bytes` / `byte_conversion`) is fully width-parameterized rather than hardcoded, this is a pure configuration choice with no code changes required elsewhere:

| `QTY_BYTES` / `PRICE_BYTES` / `ID_BYTES` | Frame size | Numeric range per field |
|---|---|---|
| 8 bytes (`long long`-width) | **30 bytes** | up to ~9.2 × 10¹⁸ |
| 4 bytes (`int`-width) | **18 bytes** | up to ~4.29 × 10⁹ |

Use the smaller, 18-byte configuration when simulating realistic order sizes/prices/IDs that comfortably fit in 32 bits — it reduces per-message bandwidth and buffer size by 40%. Use the 30-byte configuration if you need the larger numeric range. Whichever you choose, **all three config headers must be updated together**, since client, server, and subscriber all size their buffers and decode offsets from these same macros.

## JSON Configuration Files

JSON is used only for local setup/seed data — symbols the server knows about, and orders the client will replay — never for the live TCP/UDP wire traffic itself.

### `server/data/symbols.json`

Declares every symbol the server will accept orders for. Each entry needs just a `symbol_name`, which by default must be exactly `SYMBOL_BYTES` (4) characters:

```json
{
    "symbols": [
        { "symbol_name": "APPL" }
    ]
}
```

Add one object per tradable symbol. The server hashes each symbol at startup to determine which shard (and which multicast port) will own it.

### `client/data/orders.json`

An array of orders the client replays in sequence on startup. Each entry's `"type"` determines which other fields are required:

**SEND** — places a new order:
```json
{
    "type": "SEND",
    "symbol": "APPL",
    "price_level": 254,
    "quantity": 1,
    "order_type": "B"
}
```
- `symbol` — ticker, `SYMBOL_BYTES` characters.
- `price_level` — price expressed in ticks (an integer number of ticks, not a raw decimal price).
- `quantity` — order size.
- `order_type` — `"B"` for buy, anything else is treated as sell.

**MODIFY** — cancels and replaces an existing order with new price/quantity:
```json
{
    "type": "MODIFY",
    "symbol": "APPL",
    "price_level": 254,
    "quantity": 1,
    "order_type": "B",
    "order_id": 3
}
```
- Same fields as SEND, plus `order_id`, the server-assigned ID of the resting order being changed.

**KILL** — cancels an existing order:
```json
{
    "type": "KILL",
    "symbol": "APPL",
    "order_id": 3
}
```
- Only `symbol` and `order_id` are required — no price, quantity, or side.

The client walks this file top to bottom and fires each order as a separate TCP message in the order listed, so you can script a full sequence of sends, modifies, and kills to exercise the matching engine deterministically.

## Build

Each module (`client/`, `server/`, `client_subscriber/`) builds independently via CMake + vcpkg.

```powershell
.\vcpkg install boost-asio boost-system

cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Run the above from inside each module's directory (or point CMake at each `CMakeLists.txt`). Requires a C++20-capable compiler and Boost (`asio`, `system`). Outputs land in each module's `build/Debug/` folder.

## Run

Startup order matters — bring subscribers up first, then the server, then the client:

```powershell
# 1. Start one or more subscribers, to observe the live book
.\client_subscriber\build\Debug\ClientSubscriber.exe

# 2. Start the matching server (loads server/data/symbols.json)
.\server\build\Debug\HFTExchangeSimulator.exe

# 3. Run the client — replays client/data/orders.json, then stays connected
#    listening for ACK/ERROR responses
.\client\build\Debug\HFTExchangeSimulatorClient.exe
```

## Configuration Reference

| Macro | Where | Meaning |
|---|---|---|
| `SERVER_PORT` | server, client | TCP port the matching engine listens on |
| `MULTICAST_IP` / `MULTICAST_PORT` | server, subscriber | UDP multicast group used for snapshots |
| `NUM_SERVER_THREADS` | server | threads servicing TCP I/O |
| `NUM_MARKET_BOOK_THREADS` | server | threads draining the order queue and running matching |
| `NUM_SHARDS` | server | number of independent order-book shards for symbol parallelism |
| `QUEUE_SIZE` / `SNAPSHOT_QUEUE_SIZE` | server | lock-free queue capacities |
| `SNAPSHOT_LEN` | server, subscriber | book depth per side included in each snapshot |
| `SNAPSHOT_FREQUENCY` | server | emit a snapshot every N book-changing events per symbol |
| `TICK_SIZE` | server | nominal tick size for the exchange |
| `SYMBOL_BYTES` / `QTY_BYTES` / `PRICE_BYTES` / `ID_BYTES` | all three modules | wire-format field widths — see [Configuring Message Size](#configuring-message-size-1830-bytes) |
| `CONVERSION_LOGS` | server | verbose logging of decoded order fields |
| `PRINT_SYMBOL_HASHES` | server | print symbol → shard/port mapping at startup |
| `PRINT_SNAPSHOT` | subscriber | print each decoded snapshot to stdout |

## Error Codes

| Code | Meaning |
|---|---|
| 100 | Malformed Request |
| 101 | Symbol Not Found |
| 102 | Invalid Quantity |
| 103 | Invalid Price |
| 104 | Invalid Order ID |