#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		consputc(str[i]);
	}
	return size;
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	debugf("sys_read fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDIN)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
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

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	struct TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(struct TimeVal));
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!\n");
	return fork();
}

uint64 sys_exec(uint64 va)
{
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	debugf("sys_exec %s\n", name);
	return exec(name);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	return spawn(name);
}

uint64 sys_set_priority(long long prio) {
	// TODO: your job is to complete the sys call
	if (prio <= 1) return -1;
	curr_proc()->pass = BIGSTRIDE / prio;
	return prio;
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

uint64 sys_task_info(uint64 val)
{
	struct proc *p = curr_proc();
	struct TaskInfo ti;
	ti.status = RUNNING;
	memmove(ti.syscall_times, p->syscall_times, MAX_SYSCALL_NUM * sizeof(uint));
	ti.time = get_time() - p->start_time;
	copyout(p->pagetable, val, (char *)&ti, sizeof(struct TaskInfo));
	return 0;
}

uint64 sys_mmap(uint64 start, uint64 len, int prot, int flag, int fd)
{
	if (len == 0) return 0;
	if (!PGALIGNED(start)) return -1;
	if ((prot & ~7) != 0) return -1;
	if ((prot & 7) == 0) return -1;
	int perm = (prot << 1) | PTE_U;
	uint64 end = start + len;
	struct proc *p = curr_proc();
	pagetable_t pagetable = p->pagetable;
	for (uint64 va = start; va < end; va += PAGE_SIZE) {
		if (useraddr(pagetable, va) != 0) return -1;
		uint64 pa = (uint64)kalloc();
		if (pa == 0 || mappages(pagetable, va, PAGE_SIZE, pa, perm) < 0) return -1;
	}
	p->max_page = MAX(p->max_page, PGROUNDUP(end) / PAGE_SIZE);
	return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
	if (len == 0) return 0;
	if (!PGALIGNED(start)) return -1;
	uint64 end = start + len;
	struct proc *p = curr_proc();
	pagetable_t pagetable = p->pagetable;
	for (uint64 va = start; va < end; va += PAGE_SIZE) {
		pte_t *pte = walk(pagetable, va, 0);
		if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) return -1;
		uint64 pa = PTE2PA(*pte);
		kfree((void *)pa);
		*pte = 0;
	}
	if (p->max_page == PGROUNDUP(end) / PAGE_SIZE)
		p->max_page = start / PAGE_SIZE;
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
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_setpriority:
		ret = sys_set_priority(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_task_info:
		ret = sys_task_info(args[0]);
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
