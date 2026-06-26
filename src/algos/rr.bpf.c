#include "../headers/enums.h"
#include "../headers/vmlinux.h"
#include <bpf/bpf_helpers.h>

static const u64 SCHED_RR_SLICE = 100000000; // standard as per linux (in ns)

static __always_inline s32 rr_init() { return scx_bpf_create_dsq(DSQ_RR, -1); };

static __always_inline void rr_enqueue(struct task_struct *p, u64 flags) {
  scx_bpf_dsq_insert(p, DSQ_RR, SCHED_RR_SLICE, flags);
};

static __always_inline bool rr_dispatch(s32 cpu,
                                        struct task_struct *prev_task) {
  return scx_bpf_dsq_move_to_local(DSQ_RR);
}