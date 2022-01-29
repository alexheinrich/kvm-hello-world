#include <stddef.h>
#include <stdint.h>

#include "../hypercalls.h"

extern void register_syscall();

static void out(uint16_t port, uint32_t value) {
    asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static int32_t in(uint16_t port) {
    int32_t ret;
    asm("in %1,%0" :  "=a" (ret) : "Nd" (port) : "memory");

    return ret;
}

static void hc_print_str(const char *str) {
    uint32_t str_trunc = (uint32_t)((uint64_t)str & 0xFFFFFFFF);
    out(HC_PRINT_STR, str_trunc);
}

static void hc_print_int(int32_t value) {
    out(HC_PRINT_INT, (uint32_t)value);
}

static int hc_open(const char *fn) {
    uint32_t fn_uint = (uint32_t)((uint64_t)fn & 0xFFFFFFFF);

    out(HC_OPEN, fn_uint);
    
    int32_t fd = in(HC_OPEN);

    hc_print_int(fd);

    return fd;
}

static int32_t hc_close(int fd) {
    out(HC_CLOSE, (uint32_t)fd);

    return in(HC_CLOSE);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
    hc_print_int(0x30313233);

    // not working: causes IO_EXIT on port 0...
    // register_syscall();
    
    int fd = hc_open("example.dat");

    if (fd < 0) {
        hc_print_str("Error: hc_open()\n");
    }

    if (hc_close(fd) < 0) {
        hc_print_str("Error: hc_close()\n");
    }

    hc_print_str("Lorem ipsum\n");
	*(long *) 0x400 = 42;

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
