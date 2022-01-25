CC := gcc
CFLAGS := -Wall -Wextra -O2
GUEST_DIR := guest

.PHONY: run
run: kvm-hello-world
	./kvm-hello-world -p
	./kvm-hello-world -l

kvm-hello-world: kvm-hello-world.o payload.o
	$(CC) -g $^ -o $@

payload.o: payload.ld build/guest32.img.o build/guest64.img.o
	$(LD) -T $< -o $@

build/guest64.o: $(GUEST_DIR)/guest.c
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

build/guest64.img: build/guest64.o
	$(LD) -T $(GUEST_DIR)/guest.ld $^ -o $@

build/guest32.o: $(GUEST_DIR)/guest.c
	$(CC) $(CFLAGS) -m32 -ffreestanding -fno-pic -c -o $@ $^

build/guest32.img: build/guest32.o
	$(LD) -T $(GUEST_DIR)/guest.ld -m elf_i386 $^ -o $@

build/%.img.o: build/%.img
	$(LD) -b binary -r $^ -o $@

.PHONY: clean
clean:
	rm -f kvm-hello-world
	rm -f build/*
