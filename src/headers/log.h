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
};

#endif