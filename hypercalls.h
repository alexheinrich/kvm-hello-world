#define HC_VECTOR 0x8000
#define HC_READ (HC_VECTOR | 0)
#define HC_WRITE (HC_VECTOR | 1)
#define HC_OPEN (HC_VECTOR | 2)
#define HC_CLOSE (HC_VECTOR | 3)
#define HC_PRINT_STR (HC_VECTOR | 4)
#define HC_PRINT_INT (HC_VECTOR | 5)

