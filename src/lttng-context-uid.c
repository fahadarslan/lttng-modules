/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-uid.c
 *
 * LTTng real user ID context.
 *
 * Copyright (C) 2009-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *               2019 Michael Jeanson <mjeanson@efficios.com>
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <lttng/events.h>
#include <lttng/events-internal.h>
#include <lttng/tracer.h>
#include <ringbuffer/frontend_types.h>
#include <wrapper/vmalloc.h>
#include <wrapper/user_namespace.h>

static
size_t uid_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(uid_t));
	size += sizeof(uid_t);
	return size;
}

static
void uid_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		 struct lttng_kernel_ring_buffer_ctx *ctx,
		 struct lttng_kernel_channel_buffer *chan)
{
	uid_t uid;

	uid = lttng_current_uid();
	chan->ops->event_write(ctx, &uid, sizeof(uid), lttng_alignof(uid));
}

static
void uid_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	value->u.s64 = lttng_current_uid();
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("uid",
		lttng_kernel_static_type_integer_from_type(uid_t, __BYTE_ORDER, 10),
		false, false),
	uid_get_size,
	uid_record,
	uid_get_value,
	NULL, NULL);

int lttng_add_uid_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_uid_to_ctx);
