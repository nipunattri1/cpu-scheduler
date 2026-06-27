#ifndef __SCHED_MAPS_BPF_H
#define __SCHED_MAPS_BPF_H

#include "log.h"
#ifndef __VMLINUX_H__
#include "vmlinux.h"
#endif
#include <bpf/bpf_helpers.h>

struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(max_entries, 1);
  __type(key, __u32);
  __type(value, struct sched_stats);
} stats_map SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_TASK_STORAGE);
  __uint(map_flags, BPF_F_NO_PREALLOC);
  __type(key, int);
  __type(value, struct task_timing);
} task_timing_map SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_RINGBUF);
  __uint(max_entries, 64 * 1024); /* 64 KB buffer */
} switch_events SEC(".maps");

#define STARVATION_NS_THRESHOLD                                                \
  (50ULL * 1000 * 1000) /* 50ms wait = starved                                 \
                         */
#define INTERACTIVE_BURST_NS                                                   \
  (5ULL * 1000 * 1000) /* <5ms avg burst => interactive */

static __always_inline struct sched_stats *get_stats(void) {
  __u32 key = 0;
  return (struct sched_stats *)bpf_map_lookup_elem(&stats_map, &key);
}

static __always_inline void stat_add(__u64 *field, __u64 val) {
  __sync_fetch_and_add(field, val);
}
#endif