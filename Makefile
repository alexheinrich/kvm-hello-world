CC := gcc
CFLAGS := -Wall -Wextra -O2
GUEST_DIR := guest

.PHONY: run
run: kvm-hello-world
	./kvm-hello-world -l

kvm-hello-world: kvm-hello-world.o payload.o
	$(CC) -g $^ -o $@

payload.o: payload.ld build/guest64.img.o
	$(LD) -T $< -o $@

build/guest64.o: $(GUEST_DIR)/guest.c
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

build/guest64.img: build/guest64.o build/syscall_handler.o build/syscall_entry.o
	$(LD) -T $(GUEST_DIR)/guest.ld $^ -o $@

build/%.img.o: build/%.img
	$(LD) -b binary -r $^ -o $@

build/syscall_entry.o: $(GUEST_DIR)/syscall_entry.s
	$(AS) $^ -o $@

build/syscall_handler.o: $(GUEST_DIR)/syscall_handler.c
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

build/syscall_handler.img: build/syscall_handler.o
	$(LD) -T $(GUEST_DIR)/guest.ld $^ -o $@

.PHONY: clean
clean:
	rm -f kvm-hello-world
	rm -f build/*
