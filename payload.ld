SECTIONS
{
        .syscall64 0 : {
                syscall64 = .;
                build/syscall_entry.o
                syscall64_end = .;
        }
        .payload64 0 : AT(LOADADDR(.syscall64)+SIZEOF(.syscall64)) {
                guest64 = .;
                build/guest64.img.o
                guest64_end = .;
        }
}
