#pragma once

// Define Server Configurations

#define SERVER_PORT 8080
#define MULTICAST_PORT 9000
#define NUM_SERVER_THREADS 1
#define NUM_MARKET_BOOK_THREADS 1
#define MULTICAST_IP "239.192.37.42"

// Define TCP message byte sizes
#define SYMBOL_BYTES 4
#define QTY_BYTES 8
#define PRICE_BYTES 8
#define ID_BYTES 8
// Define Market Book logic
#define QUEUE_SIZE 1024
#define NUM_SHARDS 16

// Define Snapshot Configurations

#define SNAPSHOT_LEN 10
#define SNAPSHOT_QUEUE_SIZE 1024
#define SNAPSHOT_FREQUENCY 1
// Define Exchange Constants

#define TICK_SIZE 0.25

// Define Developer Debug
#define CONVERSION_LOGS false

// Define Error Codes
#define MALFORMED_REQUEST 100
#define SYMBOL_NOT_FOUND 101
#define INVALID_QUANTITY 102
#define INVALID_PRICE 103
#define INVALID_ORDER_ID 104

// Data paths
#define SYMBOL_PATH "data/symbols.json"