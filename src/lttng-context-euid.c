/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-euid.c
 *
 * LTTng effective user ID context.
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
size_t euid_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t size = 0;

	size += lib_ring_buffer_align(offset, lttng_alignof(uid_t));
	size += sizeof(uid_t);
	return size;
}

static
void euid_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
		 struct lttng_kernel_ring_buffer_ctx *ctx,
		 struct lttng_kernel_channel_buffer *chan)
{
	uid_t euid;

	euid = lttng_current_euid();
	chan->ops->event_write(ctx, &euid, sizeof(euid), lttng_alignof(euid));
}

static
void euid_get_value(void *priv,
		struct lttng_kernel_probe_ctx *lttng_probe_ctx,
		struct lttng_ctx_value *value)
{
	value->u.s64 = lttng_current_euid();
}

static const struct lttng_kernel_ctx_field *ctx_field = lttng_kernel_static_ctx_field(
	lttng_kernel_static_event_field("euid",
		lttng_kernel_static_type_integer_from_type(uid_t, __BYTE_ORDER, 10),
		false, false),
	euid_get_size,
	euid_record,
	euid_get_value,
	NULL, NULL);

int lttng_add_euid_to_ctx(struct lttng_kernel_ctx **ctx)
{
	int ret;

	if (lttng_kernel_find_context(*ctx, ctx_field->event_field->name))
		return -EEXIST;
	ret = lttng_kernel_context_append(ctx, ctx_field);
	wrapper_vmalloc_sync_mappings();
	return ret;
}
EXPORT_SYMBOL_GPL(lttng_add_euid_to_ctx);
