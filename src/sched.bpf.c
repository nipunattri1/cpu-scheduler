#include "algos/fcfs.bpf.c"
#include "algos/rr.bpf.c"
#include "headers/enums.h"
#include "headers/struct_ops.h"
#include "headers/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define DSQ_RR 2

// static u64 vtime_now;
static volatile u32 active_policy_g = POLICY_FCFS;

s32 BPF_STRUCT_OPS(sched_select_cpu, struct task_struct *p, s32 prev_cpu,
                   u64 wake_flags) {
  /*
  Select the last (hot) cpu if idle else ask kernel to provide an cpu
  */
  if (scx_bpf_test_and_clear_cpu_idle(prev_cpu)) {
    scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
    return prev_cpu;
  }
  s32 cpu;
  bool direct = false;
  cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &direct);

  if (direct) {
    scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
  }

  return cpu;
}

void BPF_STRUCT_OPS(sched_enqueue, struct task_struct *p, u64 flags) {
  // TODO: write enque functions
  switch (active_policy_g) {
  case POLICY_FCFS:
    fcfs_enqueue(p, flags);
    break;
  case POLICY_RR:
    rr_enqueue(p, flags);
    break;
  case POLICY_PRIORITY:
  default:
    // mlqfs should be here
    scx_bpf_dsq_insert(p, SCX_DSQ_GLOBAL, SCX_SLICE_DFL, flags);
    break;
  }
};

s32 BPF_STRUCT_OPS_SLEEPABLE(sched_init) {
  s32 err;
  err = fcfs_init();
  if (err)
    return err;
  err = rr_init();
  // if (err)
  //   return err;

  return err;
}

void BPF_STRUCT_OPS(sched_exit, struct scx_exit_info *ei) {
  /*
  dsq are auto removed and function can be used for logging
  (hence empty for now)
  */
}

void BPF_STRUCT_OPS(sched_dispatch, s32 cpu, struct task_struct *prev_task) {
  switch (active_policy_g) {
  case POLICY_FCFS:
    fcfs_dispatch(cpu, prev_task);
    break;
  case POLICY_RR:
    rr_dispatch(cpu, prev_task);
    break;
  case POLICY_PRIORITY:
  default:
    // Fallback or MLFQ/RR dispatch logic here
    rr_dispatch(cpu, prev_task);
    break;
  }
}

// void BPF_STRUCT_OPS(sched_tick, struct task_struct *p) {
//   if (active_policy_g == POLICY_RR){
//     rr_tick_handler(p);
//   }
// }

SEC(".struct_ops.link")
struct sched_ext_ops sched_ops = {
    .init = (void *)sched_init,
    // .tick = (void *)sched_tick,
    .enqueue = (void *)sched_enqueue,
    .select_cpu = (void *)sched_select_cpu,
    .dispatch = (void *)sched_dispatch,
    .exit = (void *)sched_exit,
    .name = "sched",
};

char _license[] SEC("license") = "GPL";