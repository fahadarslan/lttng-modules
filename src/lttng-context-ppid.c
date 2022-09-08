/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-ppid.c
 *
 * LTTng PPID context.
 *
 * Copyright (C) 2009-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <lttng/events.h>
#include <lttng/events-internal.h>
#include <ringbuffer/frontend_types.h>
#include <wrapper/vmalloc.h>
#include <lttng/tracer.h>

static
size_t ppid_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(pid_t));
	size += sizeof(pid_t);
	return size;
}

static
void ppid_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		 struct lttng_kernel_ring_buffer_ctx *ctx,
		 struct lttng_kernel_channel_buffer *chan)
{
	pid_t ppid;

	/*
	 * TODO: when we eventually add RCU subsystem instrumentation,
	 * taking the rcu read lock here will trigger RCU tracing
	 * recursively. We should modify the kernel synchronization so
	 * it synchronizes both for RCU and RCU sched, and rely on
	 * rcu_read_lock_sched_notrace.
	 */
	rcu_read_lock();
	ppid = task_tgid_nr(current->real_parent);
	rcu_read_unlock();
	chan->ops->event_write(ctx, &ppid, sizeof(ppid), lttng_alignof(ppid));
}

static
void ppid_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	pid_t ppid;

	/*
	 * TODO: when we eventually add RCU subsystem instrumentation,
	 * taking the rcu read lock here will trigger RCU tracing
	 * recursively. We should modify the kernel synchronization so
	 * it synchronizes both for RCU and RCU sched, and rely on
	 * rcu_read_lock_sched_notrace.
	 */
	rcu_read_lock();
	ppid = task_tgid_nr(current->real_parent);
	rcu_read_unlock();
	value->u.s64 = ppid;
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("ppid",
		lttng_kernel_static_type_integer_from_type(pid_t, __BYTE_ORDER, 10),
		false, false),
	ppid_get_size,
	ppid_record,
	ppid_get_value,
	NULL, NULL);

int lttng_add_ppid_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_ppid_to_ctx);
