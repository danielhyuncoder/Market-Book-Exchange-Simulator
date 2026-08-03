#pragma once

#define CLIENT_PORT 9000
#define SERVER_PORT 8080
#define SERVER_IP_ADDRESS "127.0.0.1"
#define CLIENT_IP_ADDRESS "127.0.0.1"
#define ORDERS_PATH "data/orders.json"
// DON'T EDIT
#define LD long double
#define LL long long

// Define TCP message byte sizes
#define SYMBOL_BYTES 4
#define QTY_BYTES 4
#define PRICE_BYTES 4
#define ID_BYTES 4
#define MAX_RESPONSE_BODY_BYTES (2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+2*ID_BYTES)