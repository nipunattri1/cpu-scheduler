#ifndef __SCHED_MAPS_BPF_H
#define __SCHED_MAPS_BPF_H

#include "log.h"
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

struct {
  __uint(type, BPF_MAP_TYPE_RINGBUF);
  __uint(max_entries, 1 << 12); /* 4KB */
} switch_events SEC(".maps");


struct {
  __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
  __uint(max_entries, 1);
  __type(key, __u32);
  __type(value, struct sched_stats);
} stats_map SEC(".maps");

#endif