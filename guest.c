#include <stddef.h>
#include <stdint.h>

static void printVal(uint32_t value) {
	asm("out %0,%1" : /* empty */ : "a" (value), "Nd" (0xEA) : "memory");
}

uint32_t numExits = 0;

static uint32_t getNumExits() {
    return numExits;
}

static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {
	const char *p;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p);

    printVal(0x30313233);

	*(long *) 0x400 = 42;

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");
}
