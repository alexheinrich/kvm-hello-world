#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

#include "hypercalls.h"

/* CR0 bits */
#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

#define MAX_FS 16

struct vm {
	int sys_fd;
	int fd;
	char *mem;
};

typedef struct {
    bool open;
    int hv_fd;
} fd_t;

struct vcpu {
	int fd;
	struct kvm_run *kvm_run;
};

fd_t vm_fds[MAX_FS];

static void init_vm_fds(void)
{
    for (int i = 0; i < 3; ++i) {
        vm_fds[i].open = true;
        vm_fds[i].hv_fd = 0;
    }
}

static int add_vm_fd(void)
{
    for (int i = 0; i < MAX_FS; ++i) {
        if (vm_fds[i].open == false) {
            vm_fds[i].open = true;

            return i;
        }
    }

    return -ENFILE;
}

static int hc_handle_read(struct vm *vm, struct vcpu *vcpu) {
    struct kvm_regs regs;
	if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
        return -1;
	}

    printf("Got rax: %llx\n", regs.rax);
    printf("Got rbx: %llx\n", regs.rbx);
    printf("Got rcx: %llu\n", regs.rcx);

    int vm_fd = (int)regs.rax;
    char *buf = &vm->mem[regs.rbx];
    size_t len = regs.rcx;

    printf("Got buf: %s\n", buf);

    regs.rax = (uint64_t)read(vm_fds[vm_fd].hv_fd, buf, len);

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
        return -1;
	}

    return 0;
}

static void hc_handle_close(struct kvm_run *run) {
    static ssize_t ret;
    
    if (run->io.direction == KVM_EXIT_IO_OUT) {
        uint32_t fd = 
            *(uint32_t *)((uint8_t *)run + run->io.data_offset);
        printf("Trying to close: %i\n", fd);
        vm_fds[fd].open = false;
        ret = close(vm_fds[fd].hv_fd);

    } else if (run->io.direction == KVM_EXIT_IO_IN) {
        int32_t *dest = 
            (int32_t *)((uint8_t *)run + run->io.data_offset);
        *dest = (int32_t)ret;
        run->io.size = 4;
    }
}

static void hc_handle_open(struct vm *vm, struct kvm_run *run) {
    static int fd_vm;

    if (run->io.direction == KVM_EXIT_IO_OUT) {
        fd_vm = add_vm_fd();
        if (fd_vm < 0) {
            return;
        }

        uint32_t *fn_off = 
            (uint32_t *)((uint8_t *)run + run->io.data_offset);
        char *fn = &vm->mem[*fn_off];
        int fd_hv = open(fn, O_RDONLY);

        vm_fds[fd_vm].hv_fd = fd_hv;
        *fn_off = (uint32_t)fd_vm;
    } else if (run->io.direction == KVM_EXIT_IO_IN) {
        uint32_t *dest = 
            (uint32_t *)((uint8_t *)run + run->io.data_offset);
        *dest = (uint32_t)fd_vm;
        run->io.size = 4;
    }
}

static void hc_handle_print_str(struct vm *vm, struct kvm_run *run) {
    uint32_t *value = 
        (uint32_t *)((uint8_t *)run + run->io.data_offset);
    printf("string: %s\n", &vm->mem[*value]);
}

static void hc_handle_print_int(struct kvm_run *run) {
    int32_t *value = 
        (int32_t *)((uint8_t *)run + run->io.data_offset);
    printf("value: 0x%x\n", *value);
    printf("value i: %i\n", *value);
}

void vm_init(struct vm *vm, size_t mem_size)
{
	int api_ver;
	struct kvm_userspace_memory_region memreg;

	vm->sys_fd = open("/dev/kvm", O_RDWR);
	if (vm->sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	api_ver = ioctl(vm->sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver < 0) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	if (api_ver != KVM_API_VERSION) {
		fprintf(stderr, "Got KVM api version %d, expected %d\n",
			api_ver, KVM_API_VERSION);
		exit(1);
	}

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

    if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (vm->mem == MAP_FAILED) {
		perror("mmap mem");
		exit(1);
	}

	madvise(vm->mem, mem_size, MADV_MERGEABLE);

	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0;
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)vm->mem;
        if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
                exit(1);
	}
}

void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
                exit(1);
	}

	int vcpu_mmap_size = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vcpu_mmap_size <= 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
                exit(1);
	}

	vcpu->kvm_run = mmap(NULL, (size_t)vcpu_mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, vcpu->fd, 0);
	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap kvm_run");
		exit(1);
	}
}

int run_vm(struct vm *vm, struct vcpu *vcpu, size_t sz)
{
	struct kvm_regs regs;
	uint64_t memval = 0;
    uint32_t numExits = 0;

	for (;;) {
		if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
			perror("KVM_RUN");
			exit(1);
		}
        
        ++numExits;

		switch (vcpu->kvm_run->exit_reason) {
		case KVM_EXIT_HLT:
			goto check;

		case KVM_EXIT_IO: {
            uint16_t port = vcpu->kvm_run->io.port;
            if (port & HC_VECTOR) {
                switch(port) {
                    case HC_READ:
                        if (hc_handle_read(vm, vcpu) < 0) {
                            // ioctl failed... what to do?
                            printf("ioctl in hc_handle_read failed\n");
                            exit(1);
                        }
                        break;
                    case HC_CLOSE:
                        hc_handle_close(vcpu->kvm_run);
                        break;
                    case HC_OPEN:
                        hc_handle_open(vm, vcpu->kvm_run);
                        break;
                    case HC_PRINT_STR:
                        hc_handle_print_str(vm, vcpu->kvm_run);
                        break;
                    case HC_PRINT_INT:
                        hc_handle_print_int(vcpu->kvm_run);
                        break;
                }
                continue;
            }
        }

			/* fall through */
		default:
			fprintf(stderr,	"Got exit_reason %d,"
				" expected KVM_EXIT_HLT (%d)\n",
				vcpu->kvm_run->exit_reason, KVM_EXIT_HLT);
			exit(1);
		}
	}

 check:
	if (ioctl(vcpu->fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (regs.rax != 42) {
		printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
		return 0;
	}

	memcpy(&memval, &vm->mem[0x400], sz);
	if (memval != 42) {
		printf("Wrong result: memory at 0x400 is %lld\n",
		       (unsigned long long)memval);
		return 0;
	}

	return 1;
}

extern const unsigned char guest64[], guest64_end[];

static void setup_64bit_code_segment(struct kvm_sregs *sregs)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 0,
		.s = 1, /* Code/data */
		.l = 1,
		.g = 1, /* 4KB granularity */
	};

	sregs->cs = seg;

	seg.type = 3; /* Data: read/write, accessed */
	seg.selector = 2 << 3;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

static void setup_long_mode(struct vm *vm, struct kvm_sregs *sregs)
{
	uint64_t pml4_addr = 0x2000;
	uint64_t *pml4 = (void *)(vm->mem + pml4_addr);

	uint64_t pdpt_addr = 0x3000;
	uint64_t *pdpt = (void *)(vm->mem + pdpt_addr);

	uint64_t pd_addr = 0x4000;
	uint64_t *pd = (void *)(vm->mem + pd_addr);

	pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
	pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
	pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_PS;

	sregs->cr3 = pml4_addr;
	sregs->cr4 = CR4_PAE | CR4_OSFXSR | CR4_OSXMMEXCPT;
	sregs->cr0
		= CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
	sregs->efer = EFER_LME | EFER_LMA | EFER_SCE;

	setup_64bit_code_segment(sregs);
}

int run_long_mode(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	printf("Testing 64-bit mode\n");

    if (ioctl(vcpu->fd, KVM_GET_SREGS, &sregs) < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_long_mode(vm, &sregs);

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &sregs) < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;
	/* Create stack at top of 2 MB page and grow down. */
	regs.rsp = 2 << 20;

	if (ioctl(vcpu->fd, KVM_SET_REGS, &regs) < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest64, (size_t)(guest64_end-guest64));
	return run_vm(vm, vcpu, 8);
}


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

	struct vm vm;
	struct vcpu vcpu;

    init_vm_fds();

    vm_init(&vm, 0x2000000);
	vcpu_init(&vm, &vcpu);

    return !run_long_mode(&vm, &vcpu);
}
