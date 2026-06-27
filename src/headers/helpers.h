#include "log.h"
#include "sched.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#ifdef __VMLINUX_H__
#include "vmlinux.h"
#else
#include <linux/types.h>
#endif

static const char *policy_name(__u32 p) {
  switch (p) {
  case 0:
    return "FCFS";
  case 1:
    return "RR";
  case 2:
    return "PRIORITY";
  default:
    return "UNKNOWN";
  }
}

static void print_stats(struct sched_bpf *skel) {
  int fd = bpf_map__fd(skel->maps.stats_map);
  int ncpu = libbpf_num_possible_cpus();
  struct sched_stats total = {0};
  __u32 key = 0;

  if (fd < 0 || ncpu <= 0)
    return;

  struct sched_stats vals[ncpu];
  if (bpf_map_lookup_elem(fd, &key, vals)) {
    fprintf(stderr, "failed to read stats_map\n");
    return;
  }

  for (int i = 0; i < ncpu; i++) {
    total.preemptions += vals[i].preemptions;
    total.context_switches += vals[i].context_switches;
    total.total_cpu_ticks += vals[i].total_cpu_ticks;
    total.processes_handled += vals[i].processes_handled;
  }

  printf("[STATS] preemptions=%llu ctx_switches=%llu ticks=%llu procs=%llu\n",
         (unsigned long long)total.preemptions,
         (unsigned long long)total.context_switches,
         (unsigned long long)total.total_cpu_ticks,
         (unsigned long long)total.processes_handled);
}
