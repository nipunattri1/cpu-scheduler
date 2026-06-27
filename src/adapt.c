#include "headers/adapt.h"
#include "headers/helpers.h"

int handle_switch_event(void *ctx, void *data, size_t len) {
  const struct switch_event *e = data;
  printf("==================================================\n");
  printf("[SCHED] *** POLICY SWITCH ***\n");
  printf("  Tick (ns) : %llu\n", (unsigned long long)e->tick);
  printf("  From      : %s\n", policy_name(e->from_policy));
  printf("  To        : %s\n", policy_name(e->to_policy));
  printf("  Reason    : %u\n", e->reason);
  printf("==================================================\n");
  return 0;
}

static void migrate_to(struct sched_bpf *skel, __u32 target,
                       const struct workload_metrics *m, int wt,
                       const struct policy_score *best, __u32 from) {
  /* push new target down to the BPF program via the global var */
  skel->bss->target_policy_g = target;

  printf("====================================================================="
         "=\n");
  printf("[ADAPTIVE] *** POLICY SWITCH ***\n");
  printf("  From       : %s\n", policy_name(from));
  printf("  To         : %s\n", policy_name(target));
  printf("  Confidence : %.3f\n", best->confidence);
  printf("  Workload   : %s\n", workload_name((enum workload_type)wt));
  printf("  Reason     : %s\n", best->rationale);
  printf("  Metrics    : avg_burst=%.2fms avg_wait=%.2fms cpu_util=%.2f\n",
         m->avg_burst_length_ms, m->avg_waiting_time_ms, m->cpu_utilization);
  printf("    starvation=%.3f interactive=%.3f csr=%.3f\n",
         m->starvation_pressure, m->interactive_fraction,
         m->context_switch_rate);
  printf("====================================================================="
         "=\n");
}

void evaluate_and_adapt(struct sched_bpf *skel, struct workload_metrics *m,
                        struct raw_totals *prev, double dt_sec) {
  struct raw_totals cur;

  if (read_raw_totals(skel, &cur))
    return;

//   printf("[DEBUG] eval tick: completions=%llu prev_completions=%llu "
//          "ctx=%llu prev_ctx=%llu busy=%llu prev_busy=%llu\n",
//          (unsigned long long)cur.completions,
//          (unsigned long long)prev->completions,
//          (unsigned long long)cur.context_switches,
//          (unsigned long long)prev->context_switches,
//          (unsigned long long)cur.total_cpu_ticks,
//          (unsigned long long)prev->total_cpu_ticks);

  if (cur.completions == prev->completions &&
      cur.context_switches == prev->context_switches &&
      cur.total_cpu_ticks == prev->total_cpu_ticks) {
    *prev = cur;
    return;
  }

  sample_metrics(m, &cur, prev, dt_sec);
  *prev = cur; /* slide window forward */

  __u32 current_policy = skel->bss->active_policy_g;

  struct policy_score scores[3];
  int wt = score_policies(m, current_policy, scores);

  struct policy_score best;
  if (select_policy(scores, current_policy, &best)) {
    if (best.policy != current_policy) {
      print_stats(skel);
    }
    migrate_to(skel, best.policy, m, wt, &best, current_policy);
  }
}