#ifndef SCHED_HELPERS_H
#define SCHED_HELPERS_H
#include "log.h"
#include "sched.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <math.h>
#include <string.h>
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

struct raw_totals {
  __u64 preemptions, context_switches, total_cpu_ticks, processes_handled;
  __u64 idle_ticks, completions, burst_sum_ns, wait_sum_ns, response_sum_ns;
  __u64 starvation_count, interactive_count, total_processes;
};

struct workload_metrics {
  double avg_burst_length_ms;
  double avg_waiting_time_ms;
  double avg_response_time_ms;
  double cpu_utilization;
  double context_switch_rate;
  double throughput_rate;
  double starvation_pressure;
  double interactive_fraction;
  double queue_depth_avg;
};

enum workload_type {
  WL_CPU_BOUND,
  WL_INTERACTIVE,
  WL_BATCH,
  WL_MIXED,
  WL_STRESS
};

static const char *workload_name(enum workload_type t) {
  switch (t) {
  case WL_CPU_BOUND:
    return "CPU_BOUND";
  case WL_INTERACTIVE:
    return "INTERACTIVE";
  case WL_BATCH:
    return "BATCH";
  case WL_STRESS:
    return "STRESS";
  default:
    return "MIXED";
  }
}

struct policy_score {
  __u32 policy;
  double confidence;
  char rationale[160];
};

#define EMA_ALPHA 0.3
#define CONFIDENCE_HYSTERESIS 0.15
#define EVAL_INTERVAL_SEC 1.0

static double ema(double prev, double cur, double alpha) {
  return alpha * cur + (1.0 - alpha) * prev;
}

static int read_raw_totals(struct sched_bpf *skel, struct raw_totals *out) {
  int fd = bpf_map__fd(skel->maps.stats_map);
  int ncpu = libbpf_num_possible_cpus();
  if (fd < 0 || ncpu <= 0)
    return -1;

  struct sched_stats vals[ncpu];
  __u32 key = 0;
  if (bpf_map_lookup_elem(fd, &key, vals))
    return -1;

  memset(out, 0, sizeof(*out));
  for (int i = 0; i < ncpu; i++) {
    out->preemptions += vals[i].preemptions;
    out->context_switches += vals[i].context_switches;
    out->total_cpu_ticks += vals[i].total_cpu_ticks;
    out->processes_handled += vals[i].processes_handled;
    out->idle_ticks += vals[i].idle_ticks;
    out->completions += vals[i].completions;
    out->burst_sum_ns += vals[i].burst_sum_ns;
    out->wait_sum_ns += vals[i].wait_sum_ns;
    out->response_sum_ns += vals[i].response_sum_ns;
    out->starvation_count += vals[i].starvation_count;
    out->interactive_count += vals[i].interactive_count;
    out->total_processes += vals[i].total_processes;
  }
  return 0;
}

/* queue depth: sum of nr_queued across policy DSQs (read via a tiny syscall map
   lookup isn't directly exposed to userspace for SCX dsqs, so we approximate
   using completions/processes_handled delta as a proxy if you don't expose dsq
   depth. If you add a BPF_MAP_TYPE_ARRAY snapshot of nr_queued per policy, read
   it here instead. */
static double approx_queue_depth(const struct raw_totals *cur,
                                 const struct raw_totals *prev) {
  __u64 enq = cur->processes_handled - prev->processes_handled;
  __u64 deq = cur->completions - prev->completions;
  double depth = (double)enq - (double)deq;
  return depth > 0 ? depth : 0.0;
}

static void sample_metrics(struct workload_metrics *m,
                           const struct raw_totals *cur,
                           const struct raw_totals *prev, double dt_sec) {
  __u64 d_completions = cur->completions - prev->completions;
  __u64 d_ctx = cur->context_switches - prev->context_switches;
  __u64 d_busy = cur->total_cpu_ticks - prev->total_cpu_ticks;
  __u64 d_burst_ns = cur->burst_sum_ns - prev->burst_sum_ns;
  __u64 d_wait_ns = cur->wait_sum_ns - prev->wait_sum_ns;
  __u64 d_resp_ns = cur->response_sum_ns - prev->response_sum_ns;
  __u64 d_starv = cur->starvation_count - prev->starvation_count;
  __u64 d_interactive = cur->interactive_count - prev->interactive_count;
  __u64 d_total_procs = cur->total_processes - prev->total_processes;

  int ncpu = libbpf_num_possible_cpus();
  double total_capacity_ns = dt_sec * 1e9 * (ncpu > 0 ? ncpu : 1);
  double util =
      total_capacity_ns > 0 ? (double)d_busy / total_capacity_ns : 0.0;
  if (util > 1.0)
    util = 1.0; // Keep mathematically bounded

  double avg_burst =
      d_completions ? (double)d_burst_ns / d_completions / 1e6 : 0.0;
  double avg_wait =
      d_completions ? (double)d_wait_ns / d_completions / 1e6 : 0.0;
  double avg_resp =
      d_completions ? (double)d_resp_ns / d_completions / 1e6 : 0.0;
  double tput = dt_sec > 0 ? (double)d_completions / dt_sec : 0.0;
  double csr = dt_sec > 0 ? (double)d_ctx / dt_sec : 0.0;
  double starv = d_completions ? (double)d_starv / d_completions : 0.0;
  double ifrac = d_total_procs ? (double)d_interactive / d_total_procs : 0.0;
  double qdepth = approx_queue_depth(cur, prev);

  m->avg_burst_length_ms = ema(m->avg_burst_length_ms, avg_burst, EMA_ALPHA);
  m->avg_waiting_time_ms = ema(m->avg_waiting_time_ms, avg_wait, EMA_ALPHA);
  m->avg_response_time_ms = ema(m->avg_response_time_ms, avg_resp, EMA_ALPHA);
  m->cpu_utilization = ema(m->cpu_utilization, util, EMA_ALPHA);
  m->context_switch_rate = ema(m->context_switch_rate, csr, EMA_ALPHA);
  m->throughput_rate = ema(m->throughput_rate, tput, EMA_ALPHA);
  m->starvation_pressure = ema(m->starvation_pressure, starv, EMA_ALPHA);
  m->interactive_fraction = ema(m->interactive_fraction, ifrac, EMA_ALPHA);
  m->queue_depth_avg = ema(m->queue_depth_avg, qdepth, EMA_ALPHA);
}

/* ---- Step 2: classify_workload ---- */
static enum workload_type classify_workload(const struct workload_metrics *m) {
  const double CPU_BURST_THRESHOLD = 12.0;
  const double INTERACTIVE_THRESHOLD = 0.4;
  const double STARVATION_THRESHOLD_V = 0.15;
  const double HIGH_UTIL_THRESHOLD = 0.85;

  double cpu_score = 0.0, int_score = 0.0, batch_score = 0.0;

  if (m->avg_burst_length_ms > CPU_BURST_THRESHOLD)
    cpu_score += 0.4;
  if (m->cpu_utilization > HIGH_UTIL_THRESHOLD)
    cpu_score += 0.3;
  if (m->context_switch_rate < 0.1)
    cpu_score += 0.2;
  if (m->queue_depth_avg > 5.0)
    cpu_score += 0.1;

  if (m->interactive_fraction > INTERACTIVE_THRESHOLD)
    int_score += 0.5;
  if (m->avg_response_time_ms < 5.0)
    int_score += 0.3;

  if (m->avg_burst_length_ms > 20.0)
    batch_score += 0.5;
  if (m->throughput_rate < 0.05)
    batch_score += 0.3;
  if (m->context_switch_rate < 0.05)
    batch_score += 0.2;

  if (m->starvation_pressure > STARVATION_THRESHOLD_V)
    return WL_MIXED; /* starvation overrides */

  if (cpu_score > int_score && cpu_score > batch_score)
    return (m->cpu_utilization > HIGH_UTIL_THRESHOLD) ? WL_STRESS
                                                      : WL_CPU_BOUND;
  if (int_score > cpu_score && int_score > batch_score)
    return WL_INTERACTIVE;
  if (batch_score > 0.6)
    return WL_BATCH;
  return WL_MIXED;
}

/* ---- Step 3: score_policies ---- */
static int score_policies(const struct workload_metrics *m,
                          __u32 current_policy, struct policy_score scores[3]) {
  enum workload_type wt = classify_workload(m);

  /* FCFS */
  {
    double s = 0.0;
    char r[160] = "";
    if (wt == WL_BATCH) {
      s += 0.6;
      strncat(r, "batch; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_CPU_BOUND) {
      s += 0.4;
      strncat(r, "cpu-bound; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->starvation_pressure > 0.1) {
      s -= 0.4;
      strncat(r, "starvation risk; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->interactive_fraction > 0.3) {
      s -= 0.3;
      strncat(r, "interactive present; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->queue_depth_avg > 8.0 && m->avg_burst_length_ms > 10.0)
      s -= 0.2;
    if (current_policy == 0)
      s += 0.05;
    s = fmax(0.0, fmin(1.0, s));
    scores[0] = (struct policy_score){0, s, ""};
    strncpy(scores[0].rationale, r[0] ? r : "baseline",
            sizeof(scores[0].rationale) - 1);
  }

  /* RR */
  {
    double s = 0.0;
    char r[160] = "";
    if (wt == WL_INTERACTIVE) {
      s += 0.55;
      strncat(r, "interactive; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_STRESS) {
      s += 0.30;
      strncat(r, "stress: fair share; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->avg_response_time_ms > 15.0) {
      s += 0.25;
      strncat(r, "high resp; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->interactive_fraction > 0.4) {
      s += 0.20;
      strncat(r, "high interactive frac; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_CPU_BOUND) {
      s -= 0.35;
      strncat(r, "cpu-bound; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_BATCH)
      s -= 0.30;
    if (m->context_switch_rate > 0.5 && current_policy == 1) {
      s -= 0.15;
      strncat(r, "excessive switches; ", sizeof(r) - strlen(r) - 1);
    }
    if (current_policy == 1)
      s += 0.05;
    s = fmax(0.0, fmin(1.0, s));
    scores[1] = (struct policy_score){1, s, ""};
    strncpy(scores[1].rationale, r[0] ? r : "baseline",
            sizeof(scores[1].rationale) - 1);
  }

  /* PRIORITY */
  {
    double s = 0.0;
    char r[160] = "";
    if (m->starvation_pressure > 0.10) {
      s += 0.45;
      strncat(r, "starvation detected; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_MIXED) {
      s += 0.20;
      strncat(r, "mixed; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->avg_waiting_time_ms > 20.0) {
      s += 0.15;
      strncat(r, "high avg wait; ", sizeof(r) - strlen(r) - 1);
    }
    if (m->queue_depth_avg > 4.0) {
      s += 0.10;
      strncat(r, "queue depth high; ", sizeof(r) - strlen(r) - 1);
    }
    if (wt == WL_STRESS) {
      s += 0.35;
      strncat(r, "stress: avoid starvation; ", sizeof(r) - strlen(r) - 1);
    }
    if (current_policy == 2)
      s += 0.05;
    s = fmax(0.0, fmin(1.0, s));
    scores[2] = (struct policy_score){2, s, ""};
    strncpy(scores[2].rationale, r[0] ? r : "baseline",
            sizeof(scores[2].rationale) - 1);
  }

  // Insertion sort algorithm stays identically the same...
  for (int i = 1; i < 3; i++) {
    struct policy_score key = scores[i];
    int j = i - 1;
    while (j >= 0 && scores[j].confidence < key.confidence) {
      scores[j + 1] = scores[j];
      j--;
    }
    scores[j + 1] = key;
  }
  return (int)wt;
}

/* ---- Step 4: select_policy (hysteresis-gated) ---- */
static int select_policy(const struct policy_score scores[3],
                         __u32 current_policy, struct policy_score *out_best) {
  const struct policy_score *best = &scores[0];
  if (best->policy == current_policy)
    return 0; /* no switch: best is already active */

  double current_score = 0.0;
  for (int i = 0; i < 3; i++)
    if (scores[i].policy == current_policy) {
      current_score = scores[i].confidence;
      break;
    }

  if (best->confidence - current_score < CONFIDENCE_HYSTERESIS)
    return 0;

  *out_best = *best;
  return 1;
}

static inline void print_stats(struct sched_bpf *skel) {
  struct raw_totals total;
  if (read_raw_totals(skel, &total))
    return;

  printf("[STATS] preemptions=%llu ctx_switches=%llu busy_ns=%llu procs=%llu\n",
         (unsigned long long)total.preemptions,
         (unsigned long long)total.context_switches,
         (unsigned long long)total.total_cpu_ticks,
         (unsigned long long)total.processes_handled);
}
#endif