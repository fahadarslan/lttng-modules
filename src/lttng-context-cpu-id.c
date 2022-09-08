/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-cpu-id.c
 *
 * LTTng CPU id context.
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
size_t cpu_id_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(int));
	size += sizeof(int);
	return size;
}

static
void cpu_id_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		struct lttng_kernel_ring_buffer_ctx *ctx,
		struct lttng_kernel_channel_buffer *chan)
{
	int cpu;

	cpu = ctx->priv.reserve_cpu;
	chan->ops->event_write(ctx, &cpu, sizeof(cpu), lttng_alignof(cpu));
}

static
void cpu_id_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	value->u.s64 = smp_processor_id();
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("cpu_id",
		lttng_kernel_static_type_integer_from_type(int, __BYTE_ORDER, 10),
		false, false),
	cpu_id_get_size,
	cpu_id_record,
	cpu_id_get_value,
	NULL, NULL);

int lttng_add_cpu_id_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_cpu_id_to_ctx);
