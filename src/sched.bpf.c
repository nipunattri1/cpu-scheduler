#include "algos/fcfs.bpf.c"
#include "algos/priority.bpf.c"
#include "algos/rr.bpf.c"
#include "headers/enums.h"
#include "headers/log.h"
#include "headers/maps.bpf.h"
#include "headers/struct_ops.h"
#include "headers/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

// static u64 vtime_now;
static volatile u32 active_policy_g = POLICY_FCFS;
static volatile u32 target_policy_g = POLICY_FCFS;

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
  switch (target_policy_g) {
  case POLICY_FCFS:
    fcfs_enqueue(p, flags);
    break;
  case POLICY_RR:
    rr_enqueue(p, flags);
    break;
  case POLICY_PRIORITY:
    priority_enqueue(p, flags);
    break;
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
  if (err)
    return err;
  return priority_init();
}

void BPF_STRUCT_OPS(sched_exit, struct scx_exit_info *ei) {
  /*
  dsq are auto removed and function can be used for logging
  (hence empty for now)
  */
}
static __always_inline u64 policy_to_dsq(u32 policy) {
  switch (policy) {
  case POLICY_FCFS:
    return DSQ_FCFS;
  case POLICY_RR:
    return DSQ_RR;
  default:
    return SCX_DSQ_GLOBAL;
  }
}

static __always_inline void dispatch_for_policy(u32 policy, s32 cpu,
                                                struct task_struct *prev_task) {
  switch (policy) {
  case POLICY_FCFS:
    fcfs_dispatch(cpu, prev_task);
    break;
  case POLICY_RR:
    rr_dispatch(cpu, prev_task);
    break;
  case POLICY_PRIORITY:
    priority_dispatch(cpu, prev_task);
    break;
  default:
    rr_dispatch(cpu, prev_task);
    break;
  }
}

void BPF_STRUCT_OPS(sched_dispatch, s32 cpu, struct task_struct *prev_task) {
  u32 active = active_policy_g;
  u32 target = target_policy_g;

  if (active != target) {
    u64 old_dsq = policy_to_dsq(active);
    s64 nr = scx_bpf_dsq_nr_queued(old_dsq);

    if (nr > 0) {
      dispatch_for_policy(active, cpu, prev_task);
      return;
    }

    struct switch_event *e = bpf_ringbuf_reserve(&switch_events, sizeof(*e), 0);
    if (e) {
      e->tick = bpf_ktime_get_ns();
      e->from_policy = active;
      e->to_policy = target;
      e->reason = SWITCH_REASON_MANUAL; // or whatever triggered it
      e->dsq_drained_nr = 0;
      bpf_ringbuf_submit(e, 0);
    }

    active_policy_g = target;
    active = target;
  }

  dispatch_for_policy(active, cpu, prev_task);
}

SEC(".struct_ops.link")
struct sched_ext_ops sched_ops = {
    .init = (void *)sched_init,
    .enqueue = (void *)sched_enqueue,
    .select_cpu = (void *)sched_select_cpu,
    .dispatch = (void *)sched_dispatch,
    .exit = (void *)sched_exit,
    .name = "sched",
};

char _license[] SEC("license") = "GPL";