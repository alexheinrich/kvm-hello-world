SECTIONS
{
        .payload64 0 : {
                guest64 = .;
                build/guest64.img.o
                guest64_end = .;
        }
}
