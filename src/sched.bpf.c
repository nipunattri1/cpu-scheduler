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

volatile __u32 active_policy_g = POLICY_FCFS;
volatile __u32 target_policy_g = POLICY_FCFS;
volatile __u32 draining_policy_g = (__u32)-1; /* no drain in progress */

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

void BPF_STRUCT_OPS(sched_enqueue, struct task_struct *p, u64 flags) {
  struct task_timing *tt = bpf_task_storage_get(&task_timing_map, p, NULL,
                                                BPF_LOCAL_STORAGE_GET_F_CREATE);
  u64 now = bpf_ktime_get_ns();
  if (tt) {
    if (tt->enqueue_ts == 0)
      tt->enqueue_ts = now;
    tt->last_enqueue_ts = now;
  }
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

static __always_inline u64 policy_to_dsq(u32 policy) {
  switch (policy) {
  case POLICY_FCFS:
    return DSQ_FCFS;
  case POLICY_RR:
    return DSQ_RR;
  case POLICY_PRIORITY:
    return DSQ_PRIORITY;
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

  struct sched_stats *st = get_stats();
  if (st && policy_to_dsq(active) &&
      scx_bpf_dsq_nr_queued(policy_to_dsq(active)) == 0)
    stat_add(&st->idle_ticks, 1);

  /* drain any leftover tasks from a policy we already switched away from;*/
  if (draining_policy_g != (__u32)-1) {
    u64 drain_dsq = policy_to_dsq(draining_policy_g);
    if (scx_bpf_dsq_nr_queued(drain_dsq) > 0) {
      dispatch_for_policy(draining_policy_g, cpu, prev_task);
      return;
    }
    draining_policy_g = (__u32)-1; /* fully drained */
  }

  if (active != target) {
    u64 old_dsq = policy_to_dsq(active);
    s64 nr = scx_bpf_dsq_nr_queued(old_dsq);

    /* switch immediately; any remaining old-DSQ tasks are handed off */
    if (nr > 0)
      draining_policy_g = active;

    if (__sync_val_compare_and_swap(&active_policy_g, active, target) ==
        active) {
      struct switch_event *e =
          bpf_ringbuf_reserve(&switch_events, sizeof(*e), 0);
      if (e) {
        e->tick = bpf_ktime_get_ns();
        e->from_policy = active;
        e->to_policy = target;
        e->reason = SWITCH_REASON_MANUAL;
        e->dsq_drained_nr = 0;
        bpf_ringbuf_submit(e, 0);
      }
    }
    active = target;
    active_policy_g = target;
  }

  dispatch_for_policy(active, cpu, prev_task);
}

/* task is about to start running on a CPU */
void BPF_STRUCT_OPS(sched_running, struct task_struct *p) {
  u64 now = bpf_ktime_get_ns();
  struct sched_stats *st = get_stats();
  struct task_timing *tt = bpf_task_storage_get(&task_timing_map, p, NULL,
                                                BPF_LOCAL_STORAGE_GET_F_CREATE);

  if (tt) {
    /* time spent waiting since this task was last enqueued */
    if (tt->last_enqueue_ts && now > tt->last_enqueue_ts) {
      u64 wait_ns = now - tt->last_enqueue_ts;
      if (st)
        stat_add(&st->wait_sum_ns, wait_ns);

      tt->wait_accum_ns += wait_ns;
      if (tt->wait_accum_ns > STARVATION_NS_THRESHOLD) {
        if (st)
          stat_add(&st->starvation_count, 1);
        tt->wait_accum_ns = 0; /* reset so we don't re-flag the same wait */
      }
    }

    tt->run_start_ts = now;
    if (!tt->has_run) {
      tt->first_run_ts = now;
      tt->has_run = 1;
      if (st && tt->enqueue_ts)
        stat_add(&st->response_sum_ns, now - tt->enqueue_ts);
    }
  }

  if (st)
    stat_add(&st->context_switches, 1);
}

/* task stops running (preempted, yields, or blocks) */
void BPF_STRUCT_OPS(sched_stopping, struct task_struct *p, bool runnable) {
  u64 now = bpf_ktime_get_ns();
  struct sched_stats *st = get_stats();
  struct task_timing *tt = bpf_task_storage_get(&task_timing_map, p, NULL, 0);

  u64 run_ns = 0;
  if (tt && tt->run_start_ts) {
    run_ns = now - tt->run_start_ts;
    if (st) {
      stat_add(&st->total_cpu_ticks,
               run_ns); /* now ns of busy time, not "ticks" */
      stat_add(&st->burst_sum_ns, run_ns);
    }

    /* cheap interactive heuristic: short bursts => interactive */
    if (tt) {
      tt->is_interactive = (run_ns < INTERACTIVE_BURST_NS) ? 1 : 0;
      if (tt->is_interactive && st)
        stat_add(&st->interactive_count, 1);
    }
  }

  if (runnable && st)
    stat_add(&st->preemptions, 1); /* still has work left = was preempted */
}

/* task finishes for good (exits the runqueue system) */
void BPF_STRUCT_OPS(sched_quiescent, struct task_struct *p, u64 deq_flags) {
  struct sched_stats *st = get_stats();
  if (st)
    stat_add(&st->completions, 1);
}

void BPF_STRUCT_OPS(sched_exit, struct scx_exit_info *ei) {
  /*
  dsq are auto removed and function can be used for logging
  (hence empty for now)
  */
}

SEC(".struct_ops.link")
struct sched_ext_ops sched_ops = {
    .init = (void *)sched_init,
    .enqueue = (void *)sched_enqueue,
    .select_cpu = (void *)sched_select_cpu,
    .dispatch = (void *)sched_dispatch,
    .running = (void *)sched_running,
    .stopping = (void *)sched_stopping,
    .quiescent = (void *)sched_quiescent,
    .exit = (void *)sched_exit,
    .name = "sched",
};

char _license[] SEC("license") = "GPL";