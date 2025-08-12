// moslty from gwen3-coder

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/orderbook.h"

// Static order book instance to avoid repeated allocations
static OrderBook g_orderbook;
static OrderBookSOA g_orderbook_soa;

// Helper function to skip whitespace
static const char* skip_whitespace(const char* str) {
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    return str;
}

// Helper function to parse a double from string (more efficient version)
static double parse_double(const char* start, const char* end) {
    // Create a temporary null-terminated string
    char temp[100];
    int len = end - start;
    if (len >= 100) len = 99;
    memcpy(temp, start, len);
    temp[len] = '\0';
    
    return atof(temp);
}

// Helper function to parse a single bid/ask entry
static int parse_entry(const char** json_ptr, OrderBookEntry* entries, int* count, int max_entries) {
    const char* ptr = *json_ptr;
    
    // Skip opening bracket
    if (*ptr != '[') {
        return 0;
    }
    ptr++;
    
    // Parse price (first element in array)
    ptr = skip_whitespace(ptr);
    if (*ptr != '"') {
        return 0;
    }
    ptr++;
    
    const char* price_start = ptr;
    while (*ptr != '"') ptr++;
    const char* price_end = ptr;
    ptr++;
    
    // Skip comma
    ptr = skip_whitespace(ptr);
    if (*ptr != ',') {
        return 0;
    }
    ptr++;
    
    // Parse amount (second element in array)
    ptr = skip_whitespace(ptr);
    if (*ptr != '"') {
        return 0;
    }
    ptr++;
    
    const char* amount_start = ptr;
    while (*ptr != '"') ptr++;
    const char* amount_end = ptr;
    ptr++;
    
    // Skip closing bracket
    if (*ptr != ']') {
        return 0;
    }
    ptr++;
    
    // Check for comma or end of array (skip comma if present)
    ptr = skip_whitespace(ptr);
    if (*ptr == ',') {
        ptr++;  // Skip the comma
        ptr = skip_whitespace(ptr);  // Skip any whitespace after comma
    }
    
    // Store the entry
    if (*count < max_entries) {
        entries[*count].price = parse_double(price_start, price_end);
        entries[*count].amount = parse_double(amount_start, amount_end);
        (*count)++;
    }
    
    *json_ptr = ptr;
    return 1;
}

// Helper function to parse bids array
static int parse_bids_array(const char* bids_start) {
    int bid_count = 0;
    const char* temp_ptr = bids_start;
    
    // Skip opening bracket of the array itself
    if (*temp_ptr == '[') {
        temp_ptr++;
    }
    
    while (*temp_ptr != ']' && bid_count < MAX_ORDERBOOK_ENTRIES) {
        if (!parse_entry(&temp_ptr, g_orderbook.bids, &g_orderbook.bid_count, MAX_ORDERBOOK_ENTRIES)) {
            break;
        }
        
        // Check if we should continue (look for comma or closing bracket)
        temp_ptr = skip_whitespace(temp_ptr);
        if (*temp_ptr == ',') {
            temp_ptr++;  // Skip comma
            temp_ptr = skip_whitespace(temp_ptr);  // Skip any whitespace after comma
        } else if (*temp_ptr == ']') {
            break;  // End of array
        }
        
        bid_count++;
    }
    
    return bid_count;
}

// Helper function to parse asks array
static int parse_asks_array(const char* asks_start) {
    int ask_count = 0;
    const char* temp_ptr = asks_start;
    
    // Skip opening bracket of the array itself
    if (*temp_ptr == '[') {
        temp_ptr++;
    }
    
    while (*temp_ptr != ']' && ask_count < MAX_ORDERBOOK_ENTRIES) {
        if (!parse_entry(&temp_ptr, g_orderbook.asks, &g_orderbook.ask_count, MAX_ORDERBOOK_ENTRIES)) {
            break;
        }
        
        // Check if we should continue (look for comma or closing bracket)
        temp_ptr = skip_whitespace(temp_ptr);
        if (*temp_ptr == ',') {
            temp_ptr++;  // Skip comma
            temp_ptr = skip_whitespace(temp_ptr);  // Skip any whitespace after comma
        } else if (*temp_ptr == ']') {
            break;  // End of array
        }
        
        ask_count++;
    }
    
    return ask_count;
}

// rebuilding the orderbook from a snapshot:
/*
 {"lastUpdateId":74282382772,
  "bids":[
     ["116851.33000000","14.02364000"]
    ,["116851.32000000","0.00010000"]
    ,["116851.31000000","0.00009000"]
    ,["116850.84000000","0.00010000"]
    ,["116850.83000000","0.04299000"]]
 ,"asks":[
     ["116851.34000000","0.78898000"]
    ,["116851.35000000","0.02279000"]
    ,["116851.86000000","0.00030000"]
    ,["116852.00000000","0.00305000"]
    ,["116852.07000000","0.00010000"]]
}
*/


int compare_orderbook_snapshot(OrderBook* ob, OrderBookSOA* ob_soa) {
    int err = 0;
    // compare ob and orderbooksoa
    for (int i = 0; i < ob->bid_count; i++) {
        if (ob->bids[i].price != g_orderbook_soa.bids.prices[i] ||
            ob->bids[i].amount != g_orderbook_soa.bids.amounts[i]) {
            printf("Discrepancy found in bids at index %d\n", i);
            err++ ;
        }
    }

    for (int i = 0; i < ob->ask_count; i++) {
        if (ob->asks[i].price != g_orderbook_soa.asks.prices[i] ||
            ob->asks[i].amount != g_orderbook_soa.asks.amounts[i]) {
            printf("Discrepancy found in asks at index %d\n", i);
            err++ ;
        }
    }

    if (err == 0) {
        printf("No discrepancies found between OrderBook and OrderBookSOA\n");
    } else {
        printf("%d discrepancies found between OrderBook and OrderBookSOA\n", err);
    }

    return err;

}

OrderBookPriceLevel* orderBookPriceLevel_from_simple_orderbook(OrderBook* ob) {
    // build orderbooksoa from orderbook
    for (int i = 0; i < ob->bid_count; i++) {
        g_orderbook_soa.bids.prices[i] = ob->bids[i].price;
        g_orderbook_soa.bids.amounts[i] = ob->bids[i].amount;
    }

}

OrderBookSOA* orderBookSOA_from_simple_orderbook(OrderBook* ob) {
    // build orderbooksoa from orderbook
    for (int i = 0; i < ob->bid_count; i++) {
        g_orderbook_soa.bids.prices[i] = ob->bids[i].price;
        g_orderbook_soa.bids.amounts[i] = ob->bids[i].amount;
    }
    g_orderbook_soa.bids.count = ob->bid_count;

    for (int i = 0; i < ob->ask_count; i++) {
        g_orderbook_soa.asks.prices[i] = ob->asks[i].price;
        g_orderbook_soa.asks.amounts[i] = ob->asks[i].amount;
    }
    g_orderbook_soa.asks.count = ob->ask_count;

    compare_orderbook_snapshot(ob, &g_orderbook_soa);

    return &g_orderbook_soa;
}


OrderBook* parse_orderbook_snapshot(const char* json) {
    // Initialize counts to zero
    g_orderbook.bid_count = 0;
    g_orderbook.ask_count = 0;
    
    const char* ptr = json;
    
    // Find bids array
    const char* bids_start = strstr(ptr, "\"bids\":[");
    if (bids_start) {
        // Move past "\"bids\":[" 
        bids_start += 7; // length of "\"bids\":["
        
        // Skip whitespace
        bids_start = skip_whitespace(bids_start);
        
        parse_bids_array(bids_start);
    }
    
    // Find asks array  
    const char* asks_start = strstr(ptr, "\"asks\":[");
    if (asks_start) {
        // Move past "\"asks\":[" 
        asks_start += 7; // length of "\"asks\":["
        
        // Skip whitespace
        asks_start = skip_whitespace(asks_start);
        
        parse_asks_array(asks_start);
    }
    
    return &g_orderbook;
}

// Free order book memory (no-op since we use static allocation)
void free_orderbook(OrderBook* ob) {
    // No-op - static allocation doesn't require freeing
}

// Print order book
void print_orderbook(OrderBook* ob) {
    if (!ob) return;
    
    printf("Bids:\n");
    for (int i = 0; i < ob->bid_count; i++) {
        printf("Price: %.8f, Amount: %.8f\n", ob->bids[i].price, ob->bids[i].amount);
    }
    
    printf("\nAsks:\n");
    for (int i = 0; i < ob->ask_count; i++) {
        printf("Price: %.8f, Amount: %.8f\n", ob->asks[i].price, ob->asks[i].amount);
    }
}
