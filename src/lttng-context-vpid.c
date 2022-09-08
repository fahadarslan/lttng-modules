/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-vpid.c
 *
 * LTTng vPID context.
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

static
size_t vpid_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(pid_t));
	size += sizeof(pid_t);
	return size;
}

static
void vpid_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		 struct lttng_kernel_ring_buffer_ctx *ctx,
		 struct lttng_kernel_channel_buffer *chan)
{
	pid_t vpid;

	/*
	 * nsproxy can be NULL when scheduled out of exit.
	 */
	if (!current->nsproxy)
		vpid = 0;
	else
		vpid = task_tgid_vnr(current);
	chan->ops->event_write(ctx, &vpid, sizeof(vpid), lttng_alignof(vpid));
}

static
void vpid_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	pid_t vpid;

	/*
	 * nsproxy can be NULL when scheduled out of exit.
	 */
	if (!current->nsproxy)
		vpid = 0;
	else
		vpid = task_tgid_vnr(current);
	value->u.s64 = vpid;
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("vpid",
		lttng_kernel_static_type_integer_from_type(pid_t, __BYTE_ORDER, 10),
		false, false),
	vpid_get_size,
	vpid_record,
	vpid_get_value,
	NULL, NULL);

int lttng_add_vpid_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_vpid_to_ctx);
