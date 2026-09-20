/*****************************************************************************\
 *  sched.h - kernel cpu affinity handlers
 *****************************************************************************
 *  Copyright (C) SchedMD LLC.
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#ifndef _XSCHED_H
#define _XSCHED_H

#ifdef __FreeBSD__
#include <sys/param.h> /* param.h must precede cpuset.h */
#include <sys/cpuset.h>
typedef cpuset_t cpu_set_t;
#endif

#ifdef __APPLE__
/*
 * macOS has no equivalent of Linux's sched_setaffinity()/sched_getaffinity()
 * or glibc's dynamically sized CPU_SET family
  */
#include <string.h>
#define CPU_SETSIZE 1024
typedef struct {
	unsigned long bits[CPU_SETSIZE / (8 * sizeof(unsigned long))];
} cpu_set_t;
#define CPU_ALLOC_SIZE(n) sizeof(cpu_set_t)
#define CPU_ZERO_S(setsize, set) memset((set), 0, (setsize))
#define CPU_SET_S(cpu, setsize, set) \
	((void) (setsize), \
	 (set)->bits[(cpu) / (8 * sizeof(unsigned long))] |= \
		(1UL << ((cpu) % (8 * sizeof(unsigned long)))))
#define CPU_CLR_S(cpu, setsize, set) \
	((void) (setsize), \
	 (set)->bits[(cpu) / (8 * sizeof(unsigned long))] &= \
		~(1UL << ((cpu) % (8 * sizeof(unsigned long)))))
#define CPU_ISSET_S(cpu, setsize, set) \
	((void) (setsize), \
	 !!((set)->bits[(cpu) / (8 * sizeof(unsigned long))] & \
	    (1UL << ((cpu) % (8 * sizeof(unsigned long))))))

static inline int _xsched_apple_cpu_count_s(size_t setsize, const cpu_set_t *set)
{
	size_t i;
	int count = 0;
	(void) setsize;
	for (i = 0; i < (CPU_SETSIZE / (8 * sizeof(unsigned long))); i++)
		count += __builtin_popcountl(set->bits[i]);
	return count;
}
#define CPU_COUNT_S(setsize, set) _xsched_apple_cpu_count_s(setsize, set)
#endif

#include <sched.h>

typedef struct {
	size_t max_cpus;
	size_t size;
	/* the mask is technically cpu_set_t[] */
	cpu_set_t mask; /* MUST BE LAST */
} xcpuset_t;

#define XCPU_COUNT(_mask) CPU_COUNT_S(_mask->size, &_mask->mask)
#define XCPU_ZERO(_mask) CPU_ZERO_S(_mask->size, &_mask->mask)
#define XCPU_SET(_cpu, _mask) CPU_SET_S(_cpu, _mask->size, &_mask->mask)
#define XCPU_CLR(_cpu, _mask) CPU_CLR_S(_cpu, _mask->size, &_mask->mask)
#define XCPU_ISSET(_cpu, _mask) CPU_ISSET_S(_cpu, _mask->size, &_mask->mask)

extern xcpuset_t *xcpuset_alloc(void);

/*
 * Convert a CPU bitmask to a hex string.
 *
 * IN mask - A CPU bitmask pointer.
 * RET - xmalloc'd string.
 */
extern char *task_cpuset_to_str(const xcpuset_t *mask);

/*
 * Convert a hex string to a CPU bitmask.
 *
 * IN str - A null-terminated hex string that specifies CPUs to set.
 * RET - xmalloc'd xcpuset_t structure, or NULL on error
 */
extern xcpuset_t *task_str_to_cpuset(const char *str);

/* Wrapper for sched_setaffinity() */
extern int xsetaffinity(pid_t pid, xcpuset_t *mask);

/*
 * Returns an allocated xcpuset_t structure describing the current cpu affinity.
 * IN - pid, or 0 for current process
 * RET - xmalloc'd xcpuset_t structure
 */
extern xcpuset_t *xgetaffinity(pid_t pid);

/*
 * RET CPUs set or 0 on error
 */
extern int get_assigned_cpu_count(void);

#endif
