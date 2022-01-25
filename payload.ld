SECTIONS
{
        .payload32 0 : {
                guest32 = .;
                guest32.img.o
                guest32_end = .;
        }
        .payload64 0 : AT(LOADADDR(.payload32)+SIZEOF(.payload32)) {
                guest64 = .;
                guest64.img.o
                guest64_end = .;
        }
}
