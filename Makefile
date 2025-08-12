# Compiler and flags
CC := $(shell which clang || which gcc)
CFLAGS := -Wall -O0

# Source and target
SRC := src/orderbook.c src/orderbook.s src/json_loader.c

TARGET := main

# Paths for static libwebsockets (adjust if needed)
STATIC_LIB_PATH := /usr/lib/aarch64-linux-gnu
STATIC_INC_PATH := /usr/include

.PHONY: all clean build build-static size test

all: build-json build-ws build-benchmark

build-benchmark:
	@echo "[BUILD] benchmark"
	$(CC) $(CFLAGS) -g -o benchmark src/benchmark.c

build-ws:
	@echo "[BUILD] Dynamic linking - from ws"
	$(CC) $(CFLAGS) -g -o $(TARGET)_with_ws $(SRC) src/main_with_binance_ws.c -lwebsockets -lssl -lcrypto -lz -ldl -lpthread

build-json:
	@echo "[BUILD] Dynamic linking - from json"
	$(CC) $(CFLAGS) -g -o $(TARGET)_with_json $(SRC) src/main_with_json_file.c -lwebsockets -lssl -lcrypto -lz -ldl -lpthread

build-static:
	@echo "[BUILD-STATIC] Statically linking libwebsockets..."
	$(CC) $(CFLAGS) -static -o $(TARGET)-static $(SRC) \
		-I$(STATIC_INC_PATH) $(STATIC_LIB_PATH)/libwebsockets.a \
		-lssl -lcrypto -lz -ldl -lpthread -lcap -lzstd

clean:
	@echo "[CLEAN] Removing binaries"
	rm -f $(TARGET) $(TARGET)-static test_runner

size:
	@echo "\n[SIZE] Dynamic build:"
	ls -lh $(TARGET)
	@echo "\n[SIZE] Static build:"
	ls -lh $(TARGET)-static

# Test target
test:
	@echo "[TEST] Compiling and running unit tests..."
	$(CC) $(CFLAGS) -I. -Itests -o test_runner tests/test_orderbook_parser.c tests/unity.c src/orderbook_parser.c
	./test_runner
	@echo "[TEST] Tests completed!"

# Alternative test target with more verbose output
test-verbose:
	@echo "[TEST] Compiling and running unit tests (verbose)..."
	$(CC) $(CFLAGS) -I. -Itests -o test_runner tests/test_orderbook_parser.c tests/unity.c src/orderbook_parser.c
	./test_runner
	@echo "[TEST] Tests completed!"