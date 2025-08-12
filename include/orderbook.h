#ifndef ORDERBOOK_PARSER_H
#define ORDERBOOK_PARSER_H

#include <stdio.h>
#include <stdint.h>

#define MAX_ORDERBOOK_ENTRIES 5000

// Order book entry structure
typedef struct {
    uint64_t id;
    double price;
    double amount;
} OrderBookEntry;

// Order book structure with fixed-size buffers
typedef struct {
    OrderBookEntry bids[MAX_ORDERBOOK_ENTRIES];
    OrderBookEntry asks[MAX_ORDERBOOK_ENTRIES];
    int bid_count;
    int ask_count;
} OrderBook;

// START: Order book with separate arrays for prices and amounts (SOA - Structure of Arrays)
typedef struct {
    double prices[MAX_ORDERBOOK_ENTRIES];
    double amounts[MAX_ORDERBOOK_ENTRIES];
    int count;
} SideSOA;

typedef struct {
    SideSOA bids;
    SideSOA asks;
} OrderBookSOA;
// END

// START: Order book with separate price levels
typedef struct {
    uint64_t id;
    double amount;
} Order;

typedef struct {
    int count;
    double price;
    Order entries[MAX_ORDERBOOK_ENTRIES];
} PriceLevel;

// Order book per price level 
typedef struct {
     OrderBookEntry bids[MAX_ORDERBOOK_ENTRIES];
     OrderBookEntry asks[MAX_ORDERBOOK_ENTRIES];
     int bid_count;
     int ask_count;
 } OrderBookPriceLevel;
// END

// Main parsing function - parses a complete order book snapshot
OrderBook* parse_orderbook_snapshot(const char* json);

OrderBookSOA* orderBookSOA_from_simple_orderbook(OrderBook* ob);
OrderBookPriceLevel* orderBookPriceLevel_from_simple_orderbook(OrderBook* ob);

// Free order book memory (no-op for this approach)
void free_orderbook(OrderBook* ob);

// Print order book
void print_orderbook(OrderBook* ob);

#endif // ORDERBOOK_PARSER_H