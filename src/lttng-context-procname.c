/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-procname.c
 *
 * LTTng procname context.
 *
 * Copyright (C) 2009-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <lttng/events.h>
#include <lttng/events-internal.h>
#include <ringbuffer/frontend_types.h>
#include <wrapper/vmalloc.h>
#include <lttng/tracer.h>
#include <lttng/endian.h>

static
size_t procname_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += sizeof(current->comm);
	return size;
}

/*
 * Racy read of procname. We simply copy its whole array size.
 * Races with /proc/<task>/procname write only.
 * Otherwise having to take a mutex for each event is cumbersome and
 * could lead to crash in IRQ context and deadlock of the lockdep tracer.
 */
static
void procname_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		 struct lttng_kernel_ring_buffer_ctx *ctx,
		 struct lttng_kernel_channel_buffer *chan)
{
	chan->ops->event_write(ctx, current->comm, sizeof(current->comm), 1);
}

static
void procname_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	value->u.str = current->comm;
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("procname",
		lttng_kernel_static_type_array_text(sizeof(current->comm)),
		false, false),
	procname_get_size,
	procname_record,
	procname_get_value,
	NULL, NULL);

int lttng_add_procname_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_procname_to_ctx);
