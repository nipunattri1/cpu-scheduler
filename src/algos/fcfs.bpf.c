#include "../headers/enums.h"
#include "../headers/vmlinux.h"
#include <bpf/bpf_helpers.h>

static __always_inline s32 fcfs_init() {
  return scx_bpf_create_dsq(DSQ_FCFS, -1);
};

static __always_inline void fcfs_enqueue(struct task_struct *p, u64 flags) {
  scx_bpf_dsq_insert(p, DSQ_FCFS, SCX_SLICE_DFL, flags);
};

static __always_inline void fcfs_dispatch(s32 cpu,
                                          struct task_struct *prev_task) {
  scx_bpf_dsq_move_to_local(DSQ_FCFS);
}