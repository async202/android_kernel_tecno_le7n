/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_DELAY_H
#define __ASM_GENERIC_DELAY_H

#include <linux/time64.h>

/* Undefined functions to get compile-time errors */
extern void __bad_udelay(void);
extern void __bad_ndelay(void);

extern void __udelay(unsigned long usecs);
extern void __ndelay(unsigned long nsecs);
extern void __const_udelay(unsigned long xloops);
extern void __delay(unsigned long loops);

/*
 * The weird n/20000 thing suppresses a "comparison is always false due to
 * limited range of data type" warning with non-const 8-bit arguments.
 */
#define UDELAY_CONST_MULT	((unsigned long)DIV_ROUND_UP(1ULL << 32, USEC_PER_SEC))
#define NDELAY_CONST_MULT	((unsigned long)DIV_ROUND_UP(1ULL << 32, NSEC_PER_SEC))

/*
 * The maximum constant udelay/ndelay value picked out of thin air to prevent
 * too long constant udelays/ndelays.
 */
#define DELAY_CONST_MAX   20000

/* 0x10c7 is 2**32 / 1000000 (rounded up) */
static __always_inline void udelay(unsigned long usec)
{
	if (__builtin_constant_p(usec)) {
		if (usec >= DELAY_CONST_MAX)
			__bad_udelay();
		else
			__const_udelay(usec * UDELAY_CONST_MULT);
	} else {
		__udelay(usec);
	}
}

/* 0x5 is 2**32 / 1000000000 (rounded up) */
static __always_inline void ndelay(unsigned long nsec)
{
	if (__builtin_constant_p(nsec)) {
		if (nsec >= DELAY_CONST_MAX)
			__bad_udelay();
		else
			__const_udelay(nsec * NDELAY_CONST_MULT);
	} else {
		__udelay(nsec);
	}
}
#define ndelay(x) ndelay(x)

#endif /* __ASM_GENERIC_DELAY_H */
