#include "headers/vmlinux.h"
#include <bpf/bpf_helpers.h>

#define DSQ_FCFS 1
#define DSQ_RR 2

enum policy { POLICY_FCFS, POLICY_RR, POLICY_PRIORITY, POLICY_MLFQ };

static u64 vtime_now;
static volatile u32 active_policy_g = POLICY_MLFQ;

SEC("struct_ops/adaptive_enqueue")
void adaptive_enqueue(struct task_struct *p, u64 flags) {
  // TODO: write enque functions
  switch (active_policy_g) {
  case POLICY_FCFS:
  case POLICY_RR:
  case POLICY_PRIORITY:
  default:
    // mlqfs should be here
    scx_bpf_dsq_insert(p, SCX_DSQ_GLOBAL, SCX_SLICE_DFL, flags);
    break;
  }
};

SEC("struct_ops/select_cpu")
s32 select_cpu(struct task_struct *p, s32 prev_cpu, u64 wake_flags) {
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

SEC("struct_ops.s/adaptive_init")
s32 adaptive_init(void) {
  s32 err;
  err = scx_bpf_create_dsq(DSQ_FCFS, -1);
  if (err)
    return err;
  return scx_bpf_create_dsq(DSQ_RR, -1);
}

SEC("struct_ops/adaptive_exit")
void adaptive_exit(struct scx_exit_info *ei) {}

SEC(".struct_ops.link")
struct sched_ext_ops sched_ops = {
    .select_cpu = (void *)select_cpu,
    .enqueue = (void *)adaptive_enqueue,
    .init = (void *)adaptive_init,
    .exit = (void *)adaptive_exit,
    .name = "sched",
};

char _license[] SEC("license") = "GPL";