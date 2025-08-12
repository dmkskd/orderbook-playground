#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

// Platform-specific SIMD headers
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #include <immintrin.h>  // For x86 AVX2/SSE
    #define SIMD_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>   // For ARM NEON
    #define SIMD_ARM
#else
    #define SIMD_NONE
#endif

// Common order structure
typedef struct order {
    uint64_t id;
    uint64_t price;     // Fixed-point price (e.g., price * 10000)
    uint32_t quantity;
    uint32_t timestamp;
    struct order* next;
} order_t;

// =============================================================================
// 1. SIMPLE LINKED LIST IMPLEMENTATION
// =============================================================================
typedef struct price_level {
    uint64_t price;
    uint32_t total_quantity;
    order_t* orders;        // Linked list of orders at this price
    struct price_level* next;
} price_level_t;

typedef struct simple_book {
    price_level_t* bids;    // Sorted descending (highest first)
    price_level_t* asks;    // Sorted ascending (lowest first)
} simple_book_t;

// Simple insertion - O(n) worst case
void simple_insert_order(simple_book_t* book, order_t* order, int is_bid) {
    price_level_t** head = is_bid ? &book->bids : &book->asks;
    price_level_t* current = *head;
    price_level_t* prev = NULL;
    
    // Find insertion point
    while (current && 
           ((is_bid && current->price > order->price) || 
            (!is_bid && current->price < order->price))) {
        prev = current;
        current = current->next;
    }
    
    // If price level exists, add to it
    if (current && current->price == order->price) {
        order->next = current->orders;
        current->orders = order;
        current->total_quantity += order->quantity;
        return;
    }
    
    // Create new price level
    price_level_t* new_level = malloc(sizeof(price_level_t));
    new_level->price = order->price;
    new_level->total_quantity = order->quantity;
    new_level->orders = order;
    order->next = NULL;
    new_level->next = current;
    
    if (prev) prev->next = new_level;
    else *head = new_level;
}

// =============================================================================
// 2. ARRAY-BASED IMPLEMENTATION (CACHE-FRIENDLY)
// =============================================================================
#define MAX_PRICE_LEVELS 1000
#define ORDERS_PER_LEVEL 64

typedef struct {
    uint64_t price;
    uint32_t count;
    uint32_t total_quantity;
    order_t orders[ORDERS_PER_LEVEL];  // Fixed-size array for cache locality
} array_price_level_t;

typedef struct {
    array_price_level_t bids[MAX_PRICE_LEVELS];
    array_price_level_t asks[MAX_PRICE_LEVELS];
    int bid_count;
    int ask_count;
} array_book_t;

// Binary search for price level - O(log n)
int find_price_level(array_price_level_t* levels, int count, uint64_t price, int is_bid) {
    int left = 0, right = count - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        uint64_t mid_price = levels[mid].price;
        
        if (mid_price == price) return mid;
        
        if ((is_bid && mid_price > price) || (!is_bid && mid_price < price)) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -(left + 1);  // Negative insertion point
}

void array_insert_order(array_book_t* book, order_t* order, int is_bid) {
    array_price_level_t* levels = is_bid ? book->bids : book->asks;
    int* count = is_bid ? &book->bid_count : &book->ask_count;
    
    int pos = find_price_level(levels, *count, order->price, is_bid);
    
    if (pos >= 0) {
        // Price level exists
        if (levels[pos].count < ORDERS_PER_LEVEL) {
            levels[pos].orders[levels[pos].count++] = *order;
            levels[pos].total_quantity += order->quantity;
        }
    } else {
        // Insert new price level
        pos = -(pos + 1);
        if (*count < MAX_PRICE_LEVELS) {
            // Shift elements
            memmove(&levels[pos + 1], &levels[pos], 
                   (*count - pos) * sizeof(array_price_level_t));
            
            levels[pos].price = order->price;
            levels[pos].count = 1;
            levels[pos].total_quantity = order->quantity;
            levels[pos].orders[0] = *order;
            (*count)++;
        }
    }
}

// =============================================================================
// 3. SKIP LIST IMPLEMENTATION (PROBABILISTIC)
// =============================================================================
#define MAX_SKIP_LEVEL 16

typedef struct skip_node {
    uint64_t price;
    uint32_t total_quantity;
    order_t* orders;
    struct skip_node* forward[MAX_SKIP_LEVEL];
    int level;
} skip_node_t;

typedef struct {
    skip_node_t* header;
    int level;
} skip_list_t;

typedef struct {
    skip_list_t bids;   // Descending order
    skip_list_t asks;   // Ascending order
} skip_book_t;

int random_level() {
    int level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 2) && level < MAX_SKIP_LEVEL) {
        level++;
    }
    return level;
}

skip_node_t* skip_search(skip_list_t* list, uint64_t price, int is_bid) {
    skip_node_t* current = list->header;
    
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] &&
               ((is_bid && current->forward[i]->price > price) ||
                (!is_bid && current->forward[i]->price < price))) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    return (current && current->price == price) ? current : NULL;
}

// =============================================================================
// 4. MEMORY-MAPPED PRICE ARRAY (ULTRA-FAST)
// =============================================================================
#define PRICE_RANGE 1000000     // Support prices 0-999999 (in ticks)
#define PRICE_OFFSET 500000     // Offset for negative relative prices

typedef struct {
    uint32_t total_quantity;
    uint16_t order_count;
    uint16_t reserved;
    order_t* first_order;
} direct_price_level_t;

typedef struct {
    direct_price_level_t* bid_levels;    // Array indexed by (offset - price)
    direct_price_level_t* ask_levels;    // Array indexed by (price - offset)
    uint64_t bid_top;                    // Highest bid price seen
    uint64_t ask_top;                    // Lowest ask price seen
} direct_book_t;

// O(1) insertion!
void direct_insert_order(direct_book_t* book, order_t* order, int is_bid) {
    direct_price_level_t* level;
    
    if (is_bid) {
        if (order->price >= PRICE_OFFSET) return;  // Invalid price
        level = &book->bid_levels[PRICE_OFFSET - order->price];
        if (order->price > book->bid_top) book->bid_top = order->price;
    } else {
        if (order->price >= PRICE_RANGE) return;   // Invalid price
        level = &book->ask_levels[order->price];
        if (order->price < book->ask_top || book->ask_top == 0) 
            book->ask_top = order->price;
    }
    
    order->next = level->first_order;
    level->first_order = order;
    level->total_quantity += order->quantity;
    level->order_count++;
}

// =============================================================================
// 5. SIMD-OPTIMIZED SEARCH FUNCTIONS (CROSS-PLATFORM)
// =============================================================================

#ifdef SIMD_X86
// x86 AVX2 implementation
int simd_find_price_x86(uint64_t* prices, int count, uint64_t target) {
    __m256i target_vec = _mm256_set1_epi64x(target);
    
    for (int i = 0; i <= count - 4; i += 4) {
        __m256i prices_vec = _mm256_loadu_si256((__m256i*)&prices[i]);
        __m256i cmp = _mm256_cmpeq_epi64(prices_vec, target_vec);
        
        int mask = _mm256_movemask_pd((__m256d)cmp);
        if (mask) {
            for (int j = 0; j < 4; j++) {
                if (mask & (1 << j)) return i + j;
            }
        }
    }
    
    for (int i = (count & ~3); i < count; i++) {
        if (prices[i] == target) return i;
    }
    return -1;
}

uint64_t simd_sum_quantities_x86(uint32_t* quantities, int count) {
    __m256i sum_vec = _mm256_setzero_si256();
    
    for (int i = 0; i <= count - 8; i += 8) {
        __m256i qty_vec = _mm256_loadu_si256((__m256i*)&quantities[i]);
        sum_vec = _mm256_add_epi32(sum_vec, qty_vec);
    }
    
    // Horizontal sum
    __m128i sum_128 = _mm_add_epi32(_mm256_extracti128_si256(sum_vec, 0),
                                   _mm256_extracti128_si256(sum_vec, 1));
    sum_128 = _mm_hadd_epi32(sum_128, sum_128);
    sum_128 = _mm_hadd_epi32(sum_128, sum_128);
    
    uint64_t result = _mm_extract_epi32(sum_128, 0);
    
    for (int i = (count & ~7); i < count; i++) {
        result += quantities[i];
    }
    return result;
}
#endif

#ifdef SIMD_ARM
// ARM NEON implementation
int simd_find_price_arm(uint64_t* prices, int count, uint64_t target) {
    uint64x2_t target_vec = vdupq_n_u64(target);
    
    for (int i = 0; i <= count - 2; i += 2) {
        uint64x2_t prices_vec = vld1q_u64(&prices[i]);
        uint64x2_t cmp = vceqq_u64(prices_vec, target_vec);
        
        // Check if any lane matched
        uint32x2_t cmp32 = vmovn_u64(cmp);
        if (vget_lane_u32(cmp32, 0) != 0) return i;
        if (vget_lane_u32(cmp32, 1) != 0) return i + 1;
    }
    
    // Handle remaining elements
    for (int i = (count & ~1); i < count; i++) {
        if (prices[i] == target) return i;
    }
    return -1;
}

uint64_t simd_sum_quantities_arm(uint32_t* quantities, int count) {
    uint32x4_t sum_vec = vdupq_n_u32(0);
    
    for (int i = 0; i <= count - 4; i += 4) {
        uint32x4_t qty_vec = vld1q_u32(&quantities[i]);
        sum_vec = vaddq_u32(sum_vec, qty_vec);
    }
    
    // Horizontal sum using pairwise addition
    uint32x2_t sum_pair = vadd_u32(vget_low_u32(sum_vec), vget_high_u32(sum_vec));
    uint64_t result = vget_lane_u32(vpadd_u32(sum_pair, sum_pair), 0);
    
    // Handle remaining elements
    for (int i = (count & ~3); i < count; i++) {
        result += quantities[i];
    }
    return result;
}
#endif

// Generic fallback implementations
int simd_find_price_generic(uint64_t* prices, int count, uint64_t target) {
    for (int i = 0; i < count; i++) {
        if (prices[i] == target) return i;
    }
    return -1;
}

uint64_t simd_sum_quantities_generic(uint32_t* quantities, int count) {
    uint64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += quantities[i];
    }
    return sum;
}

// Platform-agnostic wrapper functions
int simd_find_price(uint64_t* prices, int count, uint64_t target) {
#ifdef SIMD_X86
    return simd_find_price_x86(prices, count, target);
#elif defined(SIMD_ARM)
    return simd_find_price_arm(prices, count, target);
#else
    return simd_find_price_generic(prices, count, target);
#endif
}

uint64_t simd_sum_quantities(uint32_t* quantities, int count) {
#ifdef SIMD_X86
    return simd_sum_quantities_x86(quantities, count);
#elif defined(SIMD_ARM)
    return simd_sum_quantities_arm(quantities, count);
#else
    return simd_sum_quantities_generic(quantities, count);
#endif
}

// =============================================================================
// 6. LOCK-FREE CONCURRENT STRUCTURE
// =============================================================================
#include <stdatomic.h>

typedef struct lockfree_order {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    uint32_t timestamp;
    struct lockfree_order* _Atomic next;
} lockfree_order_t;

typedef struct lockfree_level {
    uint64_t price;
    _Atomic uint32_t total_quantity;
    lockfree_order_t* _Atomic orders;
    struct lockfree_level* _Atomic next;
} lockfree_level_t;

typedef struct {
    lockfree_level_t* _Atomic bids;
    lockfree_level_t* _Atomic asks;
} lockfree_book_t;

// Lock-free insertion using compare-and-swap
int lockfree_insert_order(lockfree_book_t* book, lockfree_order_t* order, int is_bid) {
    lockfree_level_t* _Atomic * head = is_bid ? &book->bids : &book->asks;
    
    while (1) {
        lockfree_level_t* current = atomic_load(head);
        lockfree_level_t* prev = NULL;
        
        // Find insertion point (simplified - full implementation needs ABA protection)
        while (current && 
               ((is_bid && current->price > order->price) ||
                (!is_bid && current->price < order->price))) {
            prev = current;
            current = atomic_load(&current->next);
        }
        
        if (current && current->price == order->price) {
            // Add to existing level
            lockfree_order_t* old_head;
            do {
                old_head = atomic_load(&current->orders);
                order->next = old_head;
            } while (!atomic_compare_exchange_weak(&current->orders, &old_head, order));
            
            atomic_fetch_add(&current->total_quantity, order->quantity);
            return 1;
        }
        
        // Create new level - simplified version
        lockfree_level_t* new_level = malloc(sizeof(lockfree_level_t));
        new_level->price = order->price;
        atomic_store(&new_level->total_quantity, order->quantity);
        atomic_store(&new_level->orders, order);
        order->next = NULL;
        atomic_store(&new_level->next, current);
        
        // Try to insert
        if (prev) {
            if (atomic_compare_exchange_strong(&prev->next, &current, new_level)) {
                return 1;
            }
        } else {
            if (atomic_compare_exchange_strong(head, &current, new_level)) {
                return 1;
            }
        }
        
        // Failed, retry
        free(new_level);
    }
}

// =============================================================================
// BENCHMARK AND COMPARISON FUNCTIONS
// =============================================================================

void print_performance_characteristics() {
    printf("ORDER BOOK DATA STRUCTURE COMPARISON:\n\n");
    
    printf("1. SIMPLE LINKED LIST:\n");
    printf("   - Insertion: O(n) worst, O(1) best (front insertion)\n");
    printf("   - Search: O(n)\n");
    printf("   - Memory: ~24 bytes/level + 32 bytes/order\n");
    printf("   - Cache: Poor (pointer chasing)\n");
    printf("   - Pros: Simple, flexible\n");
    printf("   - Cons: Slow for deep books\n\n");
    
    printf("2. ARRAY-BASED:\n");
    printf("   - Insertion: O(log n) search + O(n) shift\n");
    printf("   - Search: O(log n)\n");
    printf("   - Memory: Fixed allocation, cache-friendly\n");
    printf("   - Cache: Excellent (sequential access)\n");
    printf("   - Pros: Fast reads, good cache locality\n");
    printf("   - Cons: Expensive insertions, fixed capacity\n\n");
    
    printf("3. SKIP LIST:\n");
    printf("   - Insertion: O(log n) expected\n");
    printf("   - Search: O(log n) expected\n");
    printf("   - Memory: ~40+ bytes/level (level-dependent)\n");
    printf("   - Cache: Moderate (some pointer chasing)\n");
    printf("   - Pros: Balanced performance\n");
    printf("   - Cons: Probabilistic, complex\n\n");
    
    printf("4. DIRECT ARRAY MAPPING:\n");
    printf("   - Insertion: O(1)\n");
    printf("   - Search: O(1)\n");
    printf("   - Memory: Large fixed allocation\n");
    printf("   - Cache: Excellent\n");
    printf("   - Pros: Fastest possible operations\n");
    printf("   - Cons: Huge memory usage, limited price range\n\n");
    
    printf("5. LOCK-FREE:\n");
    printf("   - Insertion: O(log n) with retry overhead\n");
    printf("   - Search: O(log n)\n");
    printf("   - Memory: Similar to skip list + atomic overhead\n");
    printf("   - Cache: Moderate\n");
    printf("   - Pros: High concurrency\n");
    printf("   - Cons: Complex, ABA problems, retry storms\n\n");
}

// =============================================================================
// MEMORY POOL FOR EFFICIENT ORDER ALLOCATION
// =============================================================================

// Memory pool for efficient order allocation
typedef struct order_pool {
    order_t* orders;
    uint32_t* free_list;
    uint32_t capacity;
    uint32_t next_free;
} order_pool_t;

order_t* pool_alloc_order(order_pool_t* pool) {
    if (pool->next_free >= pool->capacity) return NULL;
    
    uint32_t index = pool->free_list[pool->next_free++];
    return &pool->orders[index];
}

void pool_free_order(order_pool_t* pool, order_t* order) {
    uint32_t index = order - pool->orders;
    pool->free_list[--pool->next_free] = index;
}

// =============================================================================
// BENCHMARK INFRASTRUCTURE
// =============================================================================

#include <time.h>
#include <sys/time.h>

// High precision timing
static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Generate realistic order data
typedef struct benchmark_order {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    int is_bid;
} benchmark_order_t;

void generate_orders(benchmark_order_t* orders, int count, uint64_t base_price) {
    srand(42); // Deterministic for consistent benchmarks
    
    for (int i = 0; i < count; i++) {
        orders[i].id = i;
        // Generate prices in a realistic spread around base_price
        int price_offset = (rand() % 1000) - 500; // ±500 ticks
        orders[i].price = base_price + price_offset;
        orders[i].quantity = 100 + (rand() % 10000); // 100-10100 shares
        orders[i].is_bid = rand() % 2;
    }
}

// =============================================================================
// BENCHMARK FUNCTIONS FOR EACH DATA STRUCTURE
// =============================================================================

// Simple linked list benchmark
double benchmark_simple_book(benchmark_order_t* orders, int count) {
    simple_book_t book = {0};
    
    // Pre-allocate orders in a simple array (simplified pool)
    order_t* order_storage = malloc(count * sizeof(order_t));
    
    uint64_t start = get_time_ns();
    
    for (int i = 0; i < count; i++) {
        order_t* order = &order_storage[i];
        order->id = orders[i].id;
        order->price = orders[i].price;
        order->quantity = orders[i].quantity;
        order->next = NULL;
        
        simple_insert_order(&book, order, orders[i].is_bid);
    }
    
    uint64_t end = get_time_ns();
    
    // Cleanup
    price_level_t* current = book.bids;
    while (current) {
        price_level_t* next = current->next;
        free(current);
        current = next;
    }
    current = book.asks;
    while (current) {
        price_level_t* next = current->next;
        free(current);
        current = next;
    }
    free(order_storage);
    
    return (end - start) / 1000000.0; // Convert to milliseconds
}

// Array-based benchmark
double benchmark_array_book(benchmark_order_t* orders, int count) {
    array_book_t book = {0};
    
    uint64_t start = get_time_ns();
    
    for (int i = 0; i < count; i++) {
        order_t order = {
            .id = orders[i].id,
            .price = orders[i].price,
            .quantity = orders[i].quantity
        };
        
        array_insert_order(&book, &order, orders[i].is_bid);
    }
    
    uint64_t end = get_time_ns();
    return (end - start) / 1000000.0;
}

// Direct mapping benchmark
double benchmark_direct_book(benchmark_order_t* orders, int count) {
    direct_book_t book = {0};
    
    // Allocate the large arrays
    book.bid_levels = calloc(PRICE_RANGE, sizeof(direct_price_level_t));
    book.ask_levels = calloc(PRICE_RANGE, sizeof(direct_price_level_t));
    
    if (!book.bid_levels || !book.ask_levels) {
        free(book.bid_levels);
        free(book.ask_levels);
        return -1; // Memory allocation failed
    }
    
    // Pre-allocate orders
    order_t* order_pool = malloc(count * sizeof(order_t));
    
    uint64_t start = get_time_ns();
    
    for (int i = 0; i < count; i++) {
        order_t* order = &order_pool[i];
        order->id = orders[i].id;
        order->price = orders[i].price;
        order->quantity = orders[i].quantity;
        
        // Adjust price to fit in our range
        if (orders[i].is_bid && order->price < PRICE_OFFSET) {
            direct_insert_order(&book, order, 1);
        } else if (!orders[i].is_bid && order->price < PRICE_RANGE) {
            direct_insert_order(&book, order, 0);
        }
    }
    
    uint64_t end = get_time_ns();
    
    // Cleanup
    free(book.bid_levels);
    free(book.ask_levels);
    free(order_pool);
    
    return (end - start) / 1000000.0;
}

// SIMD benchmark
double benchmark_simd_operations(uint32_t* quantities, int count) {
    // Warm up caches
    volatile uint64_t warmup = simd_sum_quantities(quantities, count);
    (void)warmup; // Suppress unused warning
    
    uint64_t start = get_time_ns();
    
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000; i++) { // Repeat for measurable time
        sum += simd_sum_quantities(quantities, count);
    }
    
    uint64_t end = get_time_ns();
    return (end - start) / 1000000.0;
}

// Generic SIMD benchmark for comparison
double benchmark_generic_operations(uint32_t* quantities, int count) {
    // Warm up caches
    volatile uint64_t warmup = simd_sum_quantities_generic(quantities, count);
    (void)warmup; // Suppress unused warning
    
    uint64_t start = get_time_ns();
    
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000; i++) { // Repeat for measurable time
        sum += simd_sum_quantities_generic(quantities, count);
    }
    
    uint64_t end = get_time_ns();
    return (end - start) / 1000000.0;
}

// Read performance benchmark (market data queries)
void benchmark_read_performance() {
    printf("=== READ PERFORMANCE BENCHMARK ===\n");
    
    const int LEVELS = 100;
    
    // Create array-based book for testing
    array_book_t book = {0};
    
    // Populate with sample data
    for (int i = 0; i < LEVELS; i++) {
        book.bids[i].price = 50000 - i; // Descending prices
        book.bids[i].total_quantity = 1000 + i * 100;
        book.asks[i].price = 50001 + i; // Ascending prices  
        book.asks[i].total_quantity = 1000 + i * 100;
    }
    book.bid_count = LEVELS;
    book.ask_count = LEVELS;
    
    // Benchmark top-of-book access (most common operation)
    uint64_t start = get_time_ns();
    volatile uint64_t best_bid_price = 0, best_ask_price = 0;
    for (int i = 0; i < 1000000; i++) {
        best_bid_price = book.bids[0].price;
        best_ask_price = book.asks[0].price;
    }
    uint64_t end = get_time_ns();
    
    // Use the variables to prevent optimization
    if (best_bid_price == 0 || best_ask_price == 0) {
        printf("Warning: benchmark may have been optimized away\n");
    }
    
    printf("Top-of-book access: %.2f ns per operation\n", 
           (end - start) / 1000000.0);
    
    // Benchmark market depth aggregation (sum top N levels)
    const int DEPTH = 10;
    start = get_time_ns();
    volatile uint64_t total_bid_qty = 0, total_ask_qty = 0;
    
    for (int i = 0; i < 100000; i++) {
        uint64_t bid_sum = 0, ask_sum = 0;
        for (int j = 0; j < DEPTH && j < book.bid_count; j++) {
            bid_sum += book.bids[j].total_quantity;
            ask_sum += book.asks[j].total_quantity;
        }
        total_bid_qty = bid_sum;
        total_ask_qty = ask_sum;
    }
    end = get_time_ns();
    
    // Use the variables to prevent optimization
    if (total_bid_qty == 0 || total_ask_qty == 0) {
        printf("Warning: depth benchmark may have been optimized away\n");
    }
    
    printf("Market depth (top-%d): %.2f ns per operation\n", 
           DEPTH, (end - start) / 100000.0);
}

// Memory usage analysis
void analyze_memory_usage() {
    printf("\n=== MEMORY USAGE ANALYSIS ===\n");
    
    const int ORDERS = 10000;
    const int LEVELS = 500;
    
    printf("For %d orders across ~%d price levels:\n\n", ORDERS, LEVELS);
    
    size_t simple_memory = LEVELS * sizeof(price_level_t) + ORDERS * sizeof(order_t);
    printf("Simple Linked List: %zu KB\n", simple_memory / 1024);
    
    size_t array_memory = sizeof(array_book_t);
    printf("Array-based: %zu KB (fixed)\n", array_memory / 1024);
    
    size_t direct_memory = 2 * PRICE_RANGE * sizeof(direct_price_level_t) + 
                          ORDERS * sizeof(order_t);
    printf("Direct Mapping: %zu MB (huge!)\n", direct_memory / (1024 * 1024));
    
    size_t skip_memory = LEVELS * sizeof(skip_node_t) + ORDERS * sizeof(order_t);
    printf("Skip List: %zu KB (estimated)\n", skip_memory / 1024);
}

// Main benchmark runner
void run_comprehensive_benchmark() {
    printf("=== COMPREHENSIVE ORDER BOOK BENCHMARK ===\n\n");
    
    const int ORDER_COUNTS[] = {1000, 5000, 10000, 25000};
    const int NUM_TESTS = sizeof(ORDER_COUNTS) / sizeof(ORDER_COUNTS[0]);
    const uint64_t BASE_PRICE = 50000;
    
    printf("%-15s", "Orders");
    printf("%-15s", "Simple(ms)");
    printf("%-15s", "Array(ms)");
    printf("%-15s", "Direct(ms)");
    printf("%-15s", "Speedup");
    printf("\n");
    printf("================================================================\n");
    
    for (int t = 0; t < NUM_TESTS; t++) {
        int count = ORDER_COUNTS[t];
        benchmark_order_t* orders = malloc(count * sizeof(benchmark_order_t));
        generate_orders(orders, count, BASE_PRICE);
        
        // Run benchmarks
        double simple_time = benchmark_simple_book(orders, count);
        double array_time = benchmark_array_book(orders, count);
        double direct_time = benchmark_direct_book(orders, count);
        
        double speedup = simple_time / direct_time;
        
        printf("%-15d", count);
        printf("%-15.2f", simple_time);
        printf("%-15.2f", array_time);
        printf("%-15.2f", direct_time);
        printf("%-15.1fx", speedup);
        printf("\n");
        
        free(orders);
    }
    
    // SIMD benchmark
    printf("\n=== SIMD PERFORMANCE ===\n");
    const int QTY_COUNT = 10000;
    uint32_t* quantities = malloc(QTY_COUNT * sizeof(uint32_t));
    
    // Ensure proper alignment for SIMD (16-byte aligned for ARM NEON)
    if ((uintptr_t)quantities % 16 != 0) {
        free(quantities);
        quantities = aligned_alloc(16, QTY_COUNT * sizeof(uint32_t));
    }
    
    for (int i = 0; i < QTY_COUNT; i++) {
        quantities[i] = 100 + (rand() % 1000);
    }
    
    double simd_time = benchmark_simd_operations(quantities, QTY_COUNT);
    double generic_time = benchmark_generic_operations(quantities, QTY_COUNT);
    
    printf("SIMD aggregation: %.2f ms (count=%d)\n", simd_time, QTY_COUNT);
    printf("Generic aggregation: %.2f ms\n", generic_time);
    printf("SIMD speedup: %.1fx\n", generic_time / simd_time);
    
#ifdef SIMD_ARM
    printf("Using ARM NEON SIMD instructions\n");
#elif defined(SIMD_X86)
    printf("Using x86 AVX2 SIMD instructions\n");
#else
    printf("No SIMD support detected\n");
#endif
    
    free(quantities);
    
    benchmark_read_performance();
    analyze_memory_usage();
}

// Example usage and testing
int main() {
    print_performance_characteristics();
    
    printf("MODERN HARDWARE CONSIDERATIONS:\n\n");
    printf("Cache Line Size: Typically 64 bytes\n");
    printf("- Pack related data in same cache line\n");
    printf("- Avoid false sharing in concurrent access\n");
    printf("- Prefetch next cache lines for sequential access\n\n");
    
    printf("SIMD Optimization Opportunities:\n");
    printf("- ARM NEON: 128-bit vectors (2x64-bit or 4x32-bit)\n");
    printf("- x86 AVX2: 256-bit vectors (4x64-bit or 8x32-bit)\n");
    printf("- x86 AVX-512: 512-bit vectors (8x64-bit or 16x32-bit)\n");
    printf("- Parallel price comparisons\n");
    printf("- Vectorized quantity aggregation\n");
    printf("- Batch order processing\n");
    printf("- Parallel market data generation\n\n");
    
    printf("Memory Access Patterns:\n");
    printf("- Sequential >> Random (10-100x faster)\n");
    printf("- Avoid pointer chasing\n");
    printf("- Use memory pools for order allocation\n");
    printf("- Consider NUMA topology for large systems\n\n");
    
    // Run the comprehensive benchmark
    run_comprehensive_benchmark();
    
    printf("\n=== PERFORMANCE PROFILING COMMANDS ===\n");
    printf("To get deeper insights, run these commands:\n\n");
    
    printf("LINUX VM PROFILING (Lima/UTM):\n");
    printf("1. CPU Performance Counters:\n");
    printf("   perf stat -e cycles,instructions,cache-misses,cache-references ./benchmark\n");
    printf("   # Shows IPC, cache hit rates\n\n");
    
    printf("2. Detailed CPU Profiling:\n");
    printf("   perf record -g ./benchmark\n");
    printf("   perf report\n");
    printf("   # Interactive call graph analysis\n\n");
    
    printf("3. Cache Performance Analysis:\n");
    printf("   perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./benchmark\n");
    printf("   # L1, L2, L3 cache performance\n\n");
    
    printf("4. Branch Prediction:\n");
    printf("   perf stat -e branch-misses,branches ./benchmark\n");
    printf("   # Should be >95%% hit rate\n\n");
    
    printf("5. Memory Bandwidth:\n");
    printf("   perf stat -e mem_load_retired.l1_miss,mem_load_retired.l1_hit ./benchmark\n");
    printf("   # Memory subsystem performance\n\n");
    
    printf("6. SIMD Instruction Analysis:\n");
    printf("   perf annotate simd_sum_quantities_arm\n");
    printf("   # See actual assembly with performance counters\n\n");
    
    printf("7. VM-Specific Checks:\n");
    printf("   cat /proc/cpuinfo | grep -E '(flags|Features)'\n");
    printf("   # Check what CPU features are exposed to VM\n");
    printf("   lscpu\n");
    printf("   # Verify VM CPU configuration\n\n");
    
    printf("8. Real-time System Monitor:\n");
    printf("   htop\n");
    printf("   # Watch CPU/memory usage during benchmark\n\n");
    
    printf("COMPILER OPTIMIZATION FOR VM:\n");
    printf("   -O3 -march=native -mtune=native -flto\n");
    printf("   # Let compiler detect VM's exposed features\n");
    printf("   -O3 -march=armv8-a -mtune=cortex-a76 -flto\n");
    printf("   # Generic ARM optimization\n\n");
    
    printf("VM PERFORMANCE TIPS:\n");
    printf("   - Increase VM memory allocation\n");
    printf("   - Enable nested virtualization if available\n");
    printf("   - Use -march=native to detect VM CPU features\n");
    printf("   - Consider running native macOS version for comparison\n\n");
    
    printf("SIMD DEBUGGING:\n");
    printf("   objdump -d benchmark | grep -A 5 -B 5 'ld1\\|add.*v[0-9]'\n");
    printf("   # Check if NEON instructions are actually generated\n");
    printf("   readelf -A benchmark\n");
    printf("   # Check binary attributes and target architecture\n\n");
    
    return 0;
}