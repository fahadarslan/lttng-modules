/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-interruptible.c
 *
 * LTTng interruptible context.
 *
 * Copyright (C) 2009-2015 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/irqflags.h>
#include <lttng/events.h>
#include <lttng/events-internal.h>
#include <ringbuffer/frontend_types.h>
#include <wrapper/vmalloc.h>
#include <lttng/tracer.h>

/*
 * Interruptible at value -1 means "unknown".
 */

static
size_t interruptible_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(int8_t));
	size += sizeof(int8_t);
	return size;
}

static
void interruptible_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		struct lttng_kernel_ring_buffer_ctx *ctx,
		struct lttng_kernel_channel_buffer *chan)
{
	struct lttng_kernel_probe_ctx *lttng_probe_ctx = ctx->probe_ctx;
	int8_t interruptible = lttng_probe_ctx->interruptible;

	chan->ops->event_write(ctx, &interruptible, sizeof(interruptible), lttng_alignof(interruptible));
}

static
void interruptible_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	int8_t interruptible = lttng_probe_ctx->interruptible;

	value->u.s64 = interruptible;
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("interruptible",
		lttng_kernel_static_type_integer_from_type(int8_t, __BYTE_ORDER, 10),
		false, false),
	interruptible_get_size,
	interruptible_record,
	interruptible_get_value,
	NULL, NULL);

int lttng_add_interruptible_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_interruptible_to_ctx);
