#include "../headers/enums.h"
#include "../headers/vmlinux.h"
#include <bpf/bpf_helpers.h>

static __always_inline s32 priority_init() {
  return scx_bpf_create_dsq(DSQ_PRIORITY, -1);
};

static __always_inline void priority_enqueue(struct task_struct *p, u64 flags) {
  s32 nice = p->static_prio - 120;
  u64 task_vtime = bpf_ktime_get_ns() + (nice * 1000000);

  scx_bpf_dsq_insert_vtime(p, DSQ_PRIORITY, SCX_SLICE_DFL, task_vtime, flags);
};

static __always_inline bool priority_dispatch(s32 cpu,
                                              struct task_struct *prev_task) {
  return scx_bpf_dsq_move_to_local(DSQ_PRIORITY);
}