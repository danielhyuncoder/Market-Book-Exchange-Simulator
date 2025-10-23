#pragma once

// Define Server Configurations

#define SERVER_PORT 8080
#define NUM_SERVER_THREADS 1
#define NUM_MARKET_BOOK_THREADS 1


// Define TCP message byte sizes
#define SYMBOL_BYTES 4
#define QTY_BYTES 8
#define PRICE_BYTES 8
#define ID_BYTES 8
// Define Market Book logic
#define QUEUE_SIZE 1024
#define NUM_SHARDS 16
// Define Exchange Constants

#define TICK_SIZE 0.25

// Define Developer Debug
#define CONVERSION_LOGS false

