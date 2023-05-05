#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define TICKS_PER_SEC (100)
// QEMU
#define CPU_FREQ (12500000)

uint64 get_cycle();
void timer_init();
void set_next_timer();
uint64 get_time();

struct TimeVal { // How to process structures in VA?
	uint64 sec; // 自 Unix 纪元起的秒数
	uint64 usec; // 微秒数
};

#endif // TIMER_H