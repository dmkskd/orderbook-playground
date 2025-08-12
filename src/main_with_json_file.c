#include "../include/orderbook.h"
#include "../include/json_loader.h"
#include <stdio.h>


int main() {
    //const char* json = "{\"lastUpdateId\":74247481137,\"bids\":[[\"116867.58000000\",\"3.43289000\"],[\"116867.57000000\",\"0.00040000\"],[\"116867.13000000\",\"0.00010000\"],[\"116867.12000000\",\"0.00035000\"],[\"116867.11000000\",\"0.28250000\"]],\"asks\":[[\"116867.59000000\",\"3.44566000\"],[\"116867.60000000\",\"0.07878000\"],[\"116867.64000000\",\"0.05198000\"],[\"116867.65000000\",\"0.05717000\"],[\"116867.77000000\",\"0.06536000\"]]}";

    char* json_data = load_json_file("data/BTCUSDT.depth_20250810.json");
    OrderBook* book = parse_orderbook_snapshot(json_data);
    printf("Order Book: bids: %d asks: %d\n", book->bid_count, book->ask_count);
    printf("Order Book: 1st bid{price: %f, amount: %f}\n", book->bids[0].price, book->bids[0].amount);
    printf("Order Book: 1st ask{price: %f, amount: %f}\n", book->asks[0].price, book->asks[0].amount);

    printf("Copying from simple orderbook into orderbook soa...\n");

    OrderBookSOA* book_soa = orderBookSOA_from_simple_orderbook(book);

    printf("Copying from simple orderbook into orderbook levels...\n");


    return 0;
}