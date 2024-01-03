#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <cstdlib>
#include <random>
#include <thread>
#include <atomic>

#define PAGESIZE 16384

#define ORDER std::memory_order_seq_cst

#if defined(__aarch64__)
    #define CTR_IDC_SHIFT           28
    #define CTR_DIC_SHIFT           29
    unsigned int cache_info = 0;
#elif defined(__riscv) and __riscv_xlen == 64
    bool riscv_local_only = false;
#endif

void my_cache_flush(void *base, void *end) {
#if defined(__aarch64__)
    // this code copied from https://github.com/gcc-mirror/gcc/blob/master/libgcc/config/aarch64/sync-cache.c
    unsigned icache_lsize;
    unsigned dcache_lsize;
    const char *address;

    /* CTR_EL0 [3:0] contains log2 of icache line size in words.
       CTR_EL0 [19:16] contains log2 of dcache line size in words.  */
         
    icache_lsize = 4 << (cache_info & 0xF);
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    /* If CTR_EL0.IDC is enabled, Data cache clean to the Point of Unification is
         not required for instruction to data coherence.  */

    if (((cache_info >> CTR_IDC_SHIFT) & 0x1) == 0x0) {
        /* Loop over the address range, clearing one cache line at once.
             Data cache must be flushed to unification first to make sure the
             instruction cache fetches the updated data.  'end' is exclusive,
             as per the GNU definition of __clear_cache.  */

        /* Make the start address of the loop cache aligned.  */
        address = (const char*) ((__UINTPTR_TYPE__) base
                     & ~ (__UINTPTR_TYPE__) (dcache_lsize - 1));

        for (; address < (const char *) end; address += dcache_lsize)
            asm volatile ("dc\tcvau, %0"
                :
                : "r" (address)
                : "memory");
    }

    asm volatile ("dsb\tish" : : : "memory");

    /* If CTR_EL0.DIC is enabled, Instruction cache cleaning to the Point of
         Unification is not required for instruction to data coherence.  */

    if (((cache_info >> CTR_DIC_SHIFT) & 0x1) == 0x0) {
        /* Make the start address of the loop cache aligned.  */
        address = (const char*) ((__UINTPTR_TYPE__) base
                     & ~ (__UINTPTR_TYPE__) (icache_lsize - 1));

        for (; address < (const char *) end; address += icache_lsize)
            asm volatile ("ic\tivau, %0"
                :
                : "r" (address)
                : "memory");

        asm volatile ("dsb\tish" : : : "memory");
    }

    asm volatile("isb" : : : "memory");
#elif defined(__riscv) and __riscv_xlen == 64
    if (riscv_local_only) asm volatile("fence.i");
    else __clear_cache(base, end);
#else
    __clear_cache(base, end);
#endif
}

void generate_set_value(int x, int *place) {
#if defined(__riscv) and __riscv_xlen == 64
    uint32_t instr[3] = {
        0x00058593, // addi a1,a1,0
        0x00b53023, // sd a1,0(a0)
        0x00008067  // ret
    };
    instr[0] |= (x & 0xfff) << 20;
    memcpy(place, instr, sizeof(instr));
    my_cache_flush(place, place + sizeof(instr));
#elif defined(__aarch64__)
    uint32_t instr[3] = {
        0x91000021, // add x1,x1,#0
        0xf9000001, // str x1,[x0]
        0xd65f03c0  // ret
    };
    instr[0] |= (x & 0xfff) << 10;
    memcpy(place, instr, sizeof(instr));
    my_cache_flush(place, place + sizeof(instr));
#else
    #error "Unsupported ISA"
#endif
}

std::atomic<bool> start_sign;
std::atomic<bool> finish_sign;
int test;

void exec(void(*instr)(int *x, int y)) {
    while (true) {
        if (start_sign.load(ORDER)) {
            start_sign.store(false, ORDER);
            instr(&test, 0);
            finish_sign.store(true, ORDER);
        }
    }
}


void init_cache_info(int argc, char *argv[]) {
#if defined(__aarch64__)
    asm volatile ("mrs\t%0, ctr_el0":"=r" (cache_info));
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-fidc") == 0) {
            cache_info |= 1<<CTR_IDC_SHIFT;
        }
        else if (strcmp(argv[i], "-fdic") == 0) {
            cache_info |= 1<<CTR_DIC_SHIFT;
        }
    }
#elif defined(__riscv) and __riscv_xlen == 64
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-flocal") == 0) {
            riscv_local_only = true;
        }
    }
#endif
}
int main(int argc, char *argv[]) {
    init_cache_info(argc, argv);

    void* addr = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (addr == NULL) {
        printf("mmap faild!\n");
        exit(1);
    }

    std::mt19937 rng;

    std::thread t1(exec, (void(*)(int *x, int y))addr);
    t1.detach();

    int res = 0;

    int loop_len = 100;
    if (argc >= 2) loop_len = atoi(argv[1]);

    for (int i=0; i<loop_len; i++) {
        int rng_res = rng() % 2048;
        generate_set_value(rng_res, (int*)addr);
        start_sign.store(true, ORDER);
        while (finish_sign.load(ORDER) == false);
        if (rng_res != test) {
            printf("%d %d\n", rng_res, test);
            res = 1;
        }
        finish_sign.store(false, ORDER);
    }
    
    return res;
}