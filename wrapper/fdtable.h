/* SPDX-License-Identifier: (GPL-2.0 or LGPL-2.1)
 *
 * wrapper/fdtable.h
 *
 * Copyright (C) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _LTTNG_WRAPPER_FDTABLE_H
#define _LTTNG_WRAPPER_FDTABLE_H

#include <lttng-kernel-version.h>
#include <linux/fdtable.h>
#include <linux/sched.h>

#if ((LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(5,11,0)) || \
	LTTNG_KERNEL_RANGE(5,10,220, 5,11,0))
static inline
struct file *lttng_lookup_fd_rcu(unsigned int fd)
{
	return lookup_fd_rcu(fd);
}
#else
static inline
struct file *lttng_lookup_fd_rcu(unsigned int fd)
{
	return fcheck(fd);
}
#endif

#if (LTTNG_LINUX_VERSION_CODE < LTTNG_KERNEL_VERSION(3,7,0))

int lttng_iterate_fd(struct files_struct *files,
		unsigned int first,
		int (*cb)(const void *, struct file *, unsigned int),
		const void *ctx);

#else

/*
 * iterate_fd() appeared at commit
 * c3c073f808b22dfae15ef8412b6f7b998644139a in the Linux kernel (first
 * released kernel: v3.7).
 */
#define lttng_iterate_fd	iterate_fd

#endif

#if (LTTNG_LINUX_VERSION_CODE >= LTTNG_KERNEL_VERSION(3,4,0))

static inline bool lttng_close_on_exec(int fd, const struct fdtable *fdt)
{
	return close_on_exec(fd, fdt);
}

#else

static inline bool lttng_close_on_exec(int fd, const struct fdtable *fdt)
{
	return FD_ISSET(fd, fdt->close_on_exec);
}

#endif

#endif /* _LTTNG_WRAPPER_FDTABLE_H */
