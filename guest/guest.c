#include <stddef.h>
#include <stdint.h>

typedef enum {
    B_LEN = 1,
    W_LEN = 2,
    L_LEN = 4
} len_t;

static void out(uint16_t port, uint32_t value, len_t len) {
    switch (len) {
        case B_LEN:
            asm("outb %0,%1" : /* empty */ : "a" ((uint8_t)value), "Nd" (port) : "memory");
            break;

        case L_LEN:
            asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
            break;

        default:
            break;
    }
}

static void printVal(uint32_t value) {
    out(0xEA, value, L_LEN);
}

static uint32_t getNumExits() {
    uint32_t numExits;
    asm("in %1, %0" : "=a" (numExits) : "Nd" (0xEB) : "memory");
    return numExits + 1;
}

static void display(const char *str) {
    uint32_t port = 0xEC;
    uint32_t str_trunc = (uint32_t)str;
    out(0xEC, str_trunc, L_LEN);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	for (p = "Hello, world!\n"; *p; ++p)
		out(0xE9, *p, B_LEN);

    printVal(0x30313233);

    printVal(getNumExits());

    display("Lorem ipsum\n");
	*(long *) 0x400 = 42;

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
