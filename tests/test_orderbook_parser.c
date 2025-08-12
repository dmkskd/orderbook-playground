// test_orderbook_parser.c
#include "unity.h"
#include "../include/orderbook_parser.h"  // Adjust path as needed

void setUp(void) {}
void tearDown(void) {}

void test_parse_empty_orderbook(void) {
    const char* json = "{\"bids\":[],\"asks\":[]}";
    OrderBook* ob = parse_orderbook_snapshot(json);
    TEST_ASSERT_NOT_NULL(ob);
    TEST_ASSERT_EQUAL_INT(0, ob->bid_count);
    TEST_ASSERT_EQUAL_INT(0, ob->ask_count);
}

void test_parse_single_bid_entry(void) {
    const char* json = "{\"bids\":[[\"49500.0\",\"1.2\"]],\"asks\":[]}";
    OrderBook* ob = parse_orderbook_snapshot(json);
    TEST_ASSERT_NOT_NULL(ob);
    TEST_ASSERT_EQUAL_INT(1, ob->bid_count);
    TEST_ASSERT_EQUAL_FLOAT(49500.0, ob->bids[0].price);
    TEST_ASSERT_EQUAL_FLOAT(1.2, ob->bids[0].amount);
}

void test_parse_single_ask_entry(void) {
    const char* json = "{\"bids\":[],\"asks\":[[\"50000.0\",\"2.3\"]]}";
    OrderBook* ob = parse_orderbook_snapshot(json);
    TEST_ASSERT_NOT_NULL(ob);
    TEST_ASSERT_EQUAL_INT(1, ob->ask_count);
    TEST_ASSERT_EQUAL_FLOAT(50000.0, ob->asks[0].price);
    TEST_ASSERT_EQUAL_FLOAT(2.3, ob->asks[0].amount);
}

void test_parse_multiple_entries(void) {
    const char* json = "{\"bids\":[[\"49500.0\",\"1.2\"],[\"49400.0\",\"1.5\"]],\"asks\":[[\"50000.0\",\"2.3\"],[\"50100.0\",\"2.7\"]]}";
    OrderBook* ob = parse_orderbook_snapshot(json);
    TEST_ASSERT_NOT_NULL(ob);
    TEST_ASSERT_EQUAL_INT(2, ob->bid_count);
    TEST_ASSERT_EQUAL_INT(2, ob->ask_count);

    TEST_ASSERT_EQUAL_FLOAT(49500.0, ob->bids[0].price);
    TEST_ASSERT_EQUAL_FLOAT(1.2, ob->bids[0].amount);

    TEST_ASSERT_EQUAL_FLOAT(50000.0, ob->asks[0].price);
    TEST_ASSERT_EQUAL_FLOAT(2.3, ob->asks[0].amount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_empty_orderbook);
    RUN_TEST(test_parse_single_bid_entry);
    RUN_TEST(test_parse_single_ask_entry);
    RUN_TEST(test_parse_multiple_entries);
    return UNITY_END();
}