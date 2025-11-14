# High-Performance Order Matching System  
**Client + Subscriber + Server**

This project implements a full exchange-like order flow:

- A **Client** that sends SEND / MODIFY / KILL orders to the server using TCP.
- A **Server** that parses, validates, processes, matches orders, produces snapshots, and multicasts them.
- A **Subscriber** that listens to multicast snapshots and decodes them.

---

# 📁 Project Structure

```
project/
│
├── client/
│   ├── data/
│   │   └── orders.json
│   ├── include/
│   │   ├── client.hpp
│   │   ├── conversions.hpp
│   │   └── client_config.h
│   ├── src/
│   │   ├── main.cpp
│
├── subscriber/
│   ├── include/
│   │   ├── subscriber.hpp
│   │   ├── enums.hpp
│   │   ├── conversions.hpp
│   │   ├── snapshot_structs.hpp
│   │   └── subscriber_config.h
│   ├── src/
│   │   ├── main.cpp
│
├── server/
│   ├── data/
│   │   └── symbols.json
│   ├── include/
│   │   ├── enums.hpp
│   │   ├── conversions.hpp
│   │   ├── server_handler.hpp
│   │   ├── server_classes.hpp
│   │   ├── order_book.hpp
│   │   └── server_config.h
│   ├── src/
│   │   ├── main.cpp
│
└── README.md
```

---

# 📦 CLIENT MODULE

## `data/orders.json`
Example order file consumed by the client:

```json
{
    "orders": [
        {
            "type": "SEND",
            "symbol": "APPL",
            "price_level": 254,
                       "quantity": 1,
            "order_type": "B"
        }
    ]
}
```

---

## Client Responsibilities
- Connect to the server via TCP.
- Encode orders using custom byte formatting.
- Pad messages to the standard TCP packet size.
- Send:
  - **SEND** order → `"O"`
  - **MODIFY** order → `"U"`
  - **KILL** order → `"X"`
- Automatically process the JSON order file.

---

## Relevant Files

### `client/include/client.hpp`
Contains:
- TCP setup  
- Order send functions  
- JSON parsing  
- ACK/ERROR decoding  
- Async listener  

### `client/include/conversions.hpp`
Provides:
- `number_to_bytes()`
- `byte_conversion()`
- `DECODE_ACK()`
- `DECODE_ERROR()`

### `client/include/client_config.h`
Defines:
- Port numbers  
- IP addresses  
- Byte widths (SYMBOL_BYTES, QTY_BYTES, etc.)

---

# 📡 SUBSCRIBER MODULE

The subscriber receives **multicast snapshots** from the server.

### Responsibilities:
- Listen on multicast IP + port.
- Decode snapshot messages into:
  - symbol name
  - sequence number
  - 10× ask orders
  - 10× bid orders
- Print snapshots when `PRINT_SNAPSHOT = true`.

### Key Files:
- `subscriber/include/conversions.hpp`
- `subscriber/include/subscriber.hpp`
- `subscriber/include/snapshot_structs.hpp`
- `subscriber/include/subscriber_config.h`

### Snapshot Structure
```
Symbol (4 bytes)
Sequence Length (8 bytes)
SELL SIDE: 10 × (ID + PRICE + QTY)
BUY SIDE: 10 × (ID + PRICE + QTY)
```

---

# ⚙️ SERVER MODULE

The server:
- Accepts client TCP order flow.
- Validates orders.
- Sends ACK / ERROR.
- Pushes orders to thread-safe lockfree queue.
- Order-book shards process orders.
- Performs matching:
  - price-time priority
  - bid/ask spreads
- Generates snapshot every `SNAPSHOT_FREQUENCY` events.
- Multicasts snapshot.

---

## Server Components

### 1. `server/include/enums.hpp`
Defines:
- `ORDER_TYPE`
- `REQUEST_TYPE`
- `ORDER`
- `OB_SNAPSHOT`

### 2. `server/include/conversions.hpp`
Decodes raw TCP messages into `ORDER`.

### 3. `server/include/order_book.hpp`
Contains:
- Order book structure
- Matching logic
- Kill / Modify logic
- Snapshot creation

### 4. `server/include/server_classes.hpp`
Contains:
- `MatchingSession` (handles TCP client)
- `OB_MCAST_FEED` (broadcast snapshots)

### 5. `server/include/server_config.h`
Defines:
- number of threads  
- snapshot frequency  
- shard count  
- byte sizes  

---

# ⚡ Matching Engine Flow

```
Client → TCP → MatchingSession → Lockfree Queue → Sharded Order Books
     → Matching Logic → Snapshot → Multicast UDP → Subscribers
```

---

# 🔧 Build & Run

## Build All:
```
Download Command (USING VCPKG AND CMAKE):
* .\vcpkg install boost-asio boost-system
(Within Project Directory): 
* cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake (Replace with your vcpkg path)
* cmake --build build
* Run executable located in ./build/Debug/HFTExchangeSimulator

```

---

# ▶️ Run Order

### 1. Start Subscriber(s)
```
./subscriber
```

### 2. Start Server
```
./server
```

### 3. Run Client (reads orders.json automatically)
```
./client
```

---

# 🔍 Notes

- All byte conversions use **little-endian → padded → reversed**.
- All packets padded to standard size:

```
2 + SYMBOL_BYTES + QTY_BYTES + PRICE_BYTES + ID_BYTES
```

- Order IDs are assigned **only** by the server.
- Snapshots broadcast every `SNAPSHOT_FREQUENCY` increments per symbol.

---

# 📚 JSON Requirements

### symbols.json
```
{
    "symbols": [
        { "symbol_name": "APPL" }
    ]
}
```

---