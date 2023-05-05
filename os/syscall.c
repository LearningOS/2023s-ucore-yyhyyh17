#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(struct TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	// YOUR CODE
	val = (struct TimeVal *)useraddr(curr_proc()->pagetable, (uint64)val);

	/* The code in `ch3` will leads to memory bugs*/

	uint64 cycle = get_cycle();
	val->sec = cycle / CPU_FREQ;
	val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
		return -1;
	return addr;
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
uint64 sys_task_info(struct TaskInfo *ti)
{
	struct proc *p = curr_proc();
	ti = (struct TaskInfo *)useraddr(p->pagetable, (uint64)ti);
	ti->status = Running;
	memmove(ti->syscall_times, p->syscall_times, MAX_SYSCALL_NUM * sizeof(uint));
	ti->time = get_time() - p->start_time;
	return 0;
}

uint64 sys_mmap(uint64 start, uint64 len, int prot, int flag, int fd)
{
	if (len == 0) return 0;
	if (!PGALIGNED(start)) return -1;
	if ((prot & ~7) != 0) return -1;
	if ((prot & 7) == 0) return -1;
	int perm = (prot << 1) | PTE_U;
	pagetable_t pagetable = curr_proc()->pagetable;
	uint64 end = start + len;
	for (uint64 va = start; va < end; va += PAGE_SIZE) {
		if (useraddr(pagetable, va) != 0) return -1;
		uint64 pa = (uint64)kalloc();
		if (pa == 0 || mappages(pagetable, va, PAGE_SIZE, pa, perm) < 0) return -1;
	}
	return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
	if (len == 0) return 0;
	if (!PGALIGNED(start)) return -1;
	pagetable_t pagetable = curr_proc()->pagetable;
	uint64 end = start + len;
	for (uint64 va = start; va < end; va += PAGE_SIZE) {
		pte_t *pte = (pte_t *)walk(pagetable, va, 0);
		if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) return -1;
		uint64 pa = PTE2PA(*pte);
		kfree((void *)pa);
		*pte = 0;
	}
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	++curr_proc()->syscall_times[id];
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((struct TimeVal *)args[0], args[1]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ret = sys_task_info((struct TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap(args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
