/* SPDX-License-Identifier: (GPL-2.0-only or LGPL-2.1-only)
 *
 * lttng-context-callstack-stackwalk-impl.h
 *
 * LTTng callstack event context, stackwalk implementation. Targets
 * kernels and architectures using the stacktrace common infrastructure
 * introduced in the upstream Linux kernel by commit 214d8ca6ee
 * "stacktrace: Provide common infrastructure" (merged in Linux 5.2,
 * then gradually introduced within architectures).
 *
 * Copyright (C) 2014-2019 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (C) 2014 Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#define MAX_ENTRIES 128

enum lttng_cs_ctx_modes {
	CALLSTACK_KERNEL = 0,
	CALLSTACK_USER = 1,
	NR_CALLSTACK_MODES,
};

struct lttng_stack_trace {
	unsigned long entries[MAX_ENTRIES];
	unsigned int nr_entries;
};

struct lttng_cs {
	struct lttng_stack_trace stack_trace[RING_BUFFER_MAX_NESTING];
};

struct field_data {
	struct lttng_cs __percpu *cs_percpu;
	enum lttng_cs_ctx_modes mode;
};

static
unsigned int (*save_func_kernel)(unsigned long *store, unsigned int size,
				unsigned int skipnr);
static
unsigned int (*save_func_user)(unsigned long *store, unsigned int size);

static
int init_type_callstack_kernel(void)
{
	unsigned long func;
	const char *func_name = "stack_trace_save";

	if (save_func_kernel)
		return 0;
	func = kallsyms_lookup_funcptr(func_name);
	if (!func) {
		printk(KERN_WARNING "LTTng: symbol lookup failed: %s\n",
				func_name);
		return -EINVAL;
	}
	save_func_kernel = (void *) func;
	return 0;
}

static
int init_type_callstack_user(void)
{
	unsigned long func;
	const char *func_name = "stack_trace_save_user";

	if (save_func_user)
		return 0;
	func = kallsyms_lookup_funcptr(func_name);
	if (!func) {
		printk(KERN_WARNING "LTTng: symbol lookup failed: %s\n",
				func_name);
		return -EINVAL;
	}
	save_func_user = (void *) func;
	return 0;
}

static
int init_type(enum lttng_cs_ctx_modes mode)
{
	switch (mode) {
	case CALLSTACK_KERNEL:
		return init_type_callstack_kernel();
	case CALLSTACK_USER:
		return init_type_callstack_user();
	default:
		return -EINVAL;
	}
}

static
void lttng_cs_set_init(struct lttng_cs __percpu *cs_set)
{
}

/* Keep track of nesting inside userspace callstack context code */
DEFINE_PER_CPU(int, callstack_user_nesting);

/*
 * Note: these callbacks expect to be invoked with preemption disabled across
 * get_size and record due to its use of a per-cpu stack.
 */
static
struct lttng_stack_trace *stack_trace_context(struct field_data *fdata, int cpu)
{
	int buffer_nesting, cs_user_nesting;
	struct lttng_cs *cs;

	/*
	 * Do not gather the userspace callstack context when the event was
	 * triggered by the userspace callstack context saving mechanism.
	 */
	cs_user_nesting = per_cpu(callstack_user_nesting, cpu);

	if (fdata->mode == CALLSTACK_USER && cs_user_nesting >= 1)
		return NULL;

	/*
	 * get_cpu() is not required, preemption is already
	 * disabled while event is written.
	 *
	 * max nesting is checked in lib_ring_buffer_get_cpu().
	 * Check it again as a safety net.
	 */
	cs = per_cpu_ptr(fdata->cs_percpu, cpu);
	buffer_nesting = per_cpu(lib_ring_buffer_nesting, cpu) - 1;
	if (buffer_nesting >= RING_BUFFER_MAX_NESTING)
		return NULL;

	return &cs->stack_trace[buffer_nesting];
}

static
size_t lttng_callstack_length_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	size_t orig_offset = offset;

	offset += lib_ring_buffer_align(offset, lttng_alignof(unsigned int));
	offset += sizeof(unsigned int);
	return offset - orig_offset;
}

/*
 * In order to reserve the correct size, the callstack is computed. The
 * resulting callstack is saved to be accessed in the record step.
 */
static
size_t lttng_callstack_sequence_get_size(void *priv, struct lttng_kernel_probe_ctx *probe_ctx, size_t offset)
{
	struct lttng_stack_trace *trace;
	struct field_data *fdata = (struct field_data *) priv;
	size_t orig_offset = offset;
	int cpu = smp_processor_id();
	struct irq_ibt_state irq_ibt_state;

	/* do not write data if no space is available */
	trace = stack_trace_context(fdata, cpu);
	if (unlikely(!trace)) {
		offset += lib_ring_buffer_align(offset, lttng_alignof(unsigned long));
		return offset - orig_offset;
	}

	/* reset stack trace, no need to clear memory */
	trace->nr_entries = 0;

	switch (fdata->mode) {
	case CALLSTACK_KERNEL:
		/* do the real work and reserve space */
		irq_ibt_state = wrapper_irq_ibt_save();
		trace->nr_entries = save_func_kernel(trace->entries,
						MAX_ENTRIES, 0);
		wrapper_irq_ibt_restore(irq_ibt_state);
		break;
	case CALLSTACK_USER:
		++per_cpu(callstack_user_nesting, cpu);
		/* do the real work and reserve space */
		irq_ibt_state = wrapper_irq_ibt_save();
		trace->nr_entries = save_func_user(trace->entries,
						MAX_ENTRIES);
		wrapper_irq_ibt_restore(irq_ibt_state);
		per_cpu(callstack_user_nesting, cpu)--;
		break;
	default:
		WARN_ON_ONCE(1);
	}

	/*
	 * If the array is filled, add our own marker to show that the
	 * stack is incomplete.
	 */
	offset += lib_ring_buffer_align(offset, lttng_alignof(unsigned long));
	offset += sizeof(unsigned long) * trace->nr_entries;
	/* Add our own ULONG_MAX delimiter to show incomplete stack. */
	if (trace->nr_entries == MAX_ENTRIES)
		offset += sizeof(unsigned long);
	return offset - orig_offset;
}

static
void lttng_callstack_length_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
			struct lttng_kernel_ring_buffer_ctx *ctx,
			struct lttng_kernel_channel_buffer *chan)
{
	int cpu = ctx->priv.reserve_cpu;
	struct field_data *fdata = (struct field_data *) priv;
	struct lttng_stack_trace *trace = stack_trace_context(fdata, cpu);
	unsigned int nr_seq_entries;

	if (unlikely(!trace)) {
		nr_seq_entries = 0;
	} else {
		nr_seq_entries = trace->nr_entries;
		if (trace->nr_entries == MAX_ENTRIES)
			nr_seq_entries++;
	}
	chan->ops->event_write(ctx, &nr_seq_entries, sizeof(unsigned int), lttng_alignof(unsigned int));
}

static
void lttng_callstack_sequence_record(void *priv, struct lttng_kernel_probe_ctx *probe_ctx,
			struct lttng_kernel_ring_buffer_ctx *ctx,
			struct lttng_kernel_channel_buffer *chan)
{
	int cpu = ctx->priv.reserve_cpu;
	struct field_data *fdata = (struct field_data *) priv;
	struct lttng_stack_trace *trace = stack_trace_context(fdata, cpu);
	unsigned int nr_seq_entries;

	if (unlikely(!trace)) {
		/* We need to align even if there are 0 elements. */
		lib_ring_buffer_align_ctx(ctx, lttng_alignof(unsigned long));
		return;
	}
	nr_seq_entries = trace->nr_entries;
	if (trace->nr_entries == MAX_ENTRIES)
		nr_seq_entries++;
	chan->ops->event_write(ctx, trace->entries,
			sizeof(unsigned long) * trace->nr_entries, lttng_alignof(unsigned long));
	/* Add our own ULONG_MAX delimiter to show incomplete stack. */
	if (trace->nr_entries == MAX_ENTRIES) {
		unsigned long delim = ULONG_MAX;

		chan->ops->event_write(ctx, &delim, sizeof(unsigned long), 1);
	}
}
