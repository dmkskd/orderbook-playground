.text
.globl _build_orderbook_asm_clean

_build_orderbook_asm_clean:
    // x0 = pointer to C OrderBook structure (source)
    // x1 = pointer to ASM OrderBook structure (destination)
    
    // Save registers
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    
    // Copy bid_count (4 bytes) - use register arithmetic with smaller offsets
    // We'll calculate addresses using a different approach
    mov x2, #32000        // Load offset value into register
    add x2, x0, x2        // Calculate address of bid_count in C structure  
    ldr w3, [x2]          // Load bid_count
    mov x4, #32000        // Load offset value into register
    add x4, x1, x4        // Calculate address of bid_count in ASM structure
    str w3, [x4]          // Store bid_count
    
    // Copy ask_count (4 bytes)
    mov x2, #32004        // Load offset value into register
    add x2, x0, x2        // Calculate address of ask_count in C structure
    ldr w3, [x2]          // Load ask_count
    mov x4, #32004        // Load offset value into register
    add x4, x1, x4        // Calculate address of ask_count in ASM structure  
    str w3, [x4]          // Store ask_count
    
    // Copy bids array - 1000 * 16 = 16000 bytes
    mov x2, x0            // Save base pointer for C structure
    mov x3, x1            // Save base pointer for ASM structure
    // bids start at offset 0 (relative to structure base)
    
    mov w4, #0            // Counter for entries
    mov w5, #1000         // Max entries
    
copy_bids_loop:
    cmp w4, w5
    bge copy_asks
    
    // Copy one bid entry (16 bytes = 2 doubles)
    ldp x6, x7, [x2]      // Load price and amount from C bids[i]
    stp x6, x7, [x3]      // Store in ASM bids[i]
    
    add x2, x2, #16       // Move to next C bid entry
    add x3, x3, #16       // Move to next ASM bid entry  
    add w4, w4, #1        // Increment counter
    b copy_bids_loop

copy_asks:
    // Copy asks array - 1000 * 16 = 16000 bytes from offset 16000
    mov x2, x0            // Save base pointer for C structure  
    mov x3, x1            // Save base pointer for ASM structure
    
    // Calculate addresses using register arithmetic (smaller immediate values)
    mov x4, #16000        // Load offset value into register
    add x2, x2, x4        // Add to get C asks address  
    add x3, x3, x4        // Add to get ASM asks address
    
    mov w4, #0            // Counter for entries
    mov w5, #1000         // Max entries
    
copy_asks_loop:
    cmp w4, w5
    bge done_copy
    
    // Copy one ask entry (16 bytes = 2 doubles)
    ldp x6, x7, [x2]      // Load price and amount from C asks[i]
    stp x6, x7, [x3]      // Store in ASM asks[i]
    
    add x2, x2, #16       // Move to next C ask entry
    add x3, x3, #16       // Move to next ASM ask entry  
    add w4, w4, #1        // Increment counter
    b copy_asks_loop

done_copy:
    ldp x29, x30, [sp], #16
    ret
