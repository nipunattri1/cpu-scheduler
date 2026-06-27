#ifndef SCHED_LOG_H
#define SCHED_LOG_H

#ifdef __VMLINUX_H__
#include "vmlinux.h"
#else
#include <linux/types.h>
#endif

enum switch_reason {
  SWITCH_REASON_MANUAL = 0,
  SWITCH_REASON_AUTO_LOAD,
  SWITCH_REASON_AUTO_LATENCY,
};

struct switch_event {
  __u64 tick;
  __u32 from_policy;
  __u32 to_policy;
  __u32 reason;
  __u32 dsq_drained_nr;
};

struct sched_stats {
  __u64 preemptions;
  __u64 context_switches;
  __u64 total_cpu_ticks;
  __u64 processes_handled;
  __u64 idle_ticks;
  __u64 completions;
  __u64 burst_sum_ns;
  __u64 wait_sum_ns;
  __u64 response_sum_ns;
  __u64 starvation_count;
  __u64 interactive_count;
  __u64 total_processes;
};
struct task_timing {
  __u64 enqueue_ts;   /* set once, first enqueue (for response time) */
  __u64 last_enqueue_ts; /* set on every enqueue (for per-cycle wait time) */
  __u64 run_start_ts; /* set in sched_running */
  __u64 first_run_ts; /* set once, for response time */
  __u8 has_run;       /* whether first_run_ts is valid */
  __u8 is_interactive;
  __u64 wait_accum_ns; /* running total of time spent waiting (for starvation
                          check) */
};
#endif