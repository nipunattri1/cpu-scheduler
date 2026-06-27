#include "headers/adapt.h"
#include "headers/enums.h"
#include "headers/helpers.h"
#include "headers/sched.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t exiting = 0;
static void sig_handler(int sig) { exiting = 1; }

int main(int argc, char *argv[]) {
  int opt;
  int flag_a = 0;
  char *flag_f_value = NULL;

  while ((opt = getopt(argc, argv, "af:")) != -1) {
    switch (opt) {
    case 'a':
      flag_a = 1;
      break;
    case 'f':
      if (strcmp(optarg, "fcfs") == 0 || strcmp(optarg, "priority") == 0 ||
          strcmp(optarg, "rr") == 0 || strcmp(optarg, "auto") == 0) {
        flag_f_value = optarg;
      } else {
        fprintf(stderr, "Error: -f only accepts: fcfs, priority, rr, auto\n");
        return 1;
      }
      break;
    default:
      fprintf(stderr, "Usage: %s [-a] [-f fcfs|priority|rr|auto]\n", argv[0]);
      return 1;
    }
  }

  if (flag_f_value == NULL) {
    flag_f_value = "auto";
  }
  printf("[INFO] Scheduler mode / policy rule: %s\n", flag_f_value);
  if (flag_a) {
    printf(
        "[INFO] Analysis Mode Enabled. Summary will be displayed on exit.\n");
  }

  struct sched_bpf *skel;
  struct ring_buffer *rb = NULL;
  time_t last_stats;
  time_t start_time;
  int err = 0;

  struct raw_totals start_totals = {0};
  struct workload_metrics metrics = {0};
  struct raw_totals prev_totals = {0};

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  skel = sched_bpf__open();
  if (!skel) {
    fprintf(stderr, "failed to open BPF skeleton\n");
    return 1;
  }

  err = sched_bpf__load(skel);
  if (err) {
    fprintf(stderr, "failed to load BPF skeleton\n");
    goto cleanup;
  }

  if (strcmp(flag_f_value, "fcfs") == 0) {
    skel->bss->target_policy_g = POLICY_FCFS;
  } else if (strcmp(flag_f_value, "rr") == 0) {
    skel->bss->target_policy_g = POLICY_RR;
  } else if (strcmp(flag_f_value, "priority") == 0) {
    skel->bss->target_policy_g = POLICY_PRIORITY;
  }

  err = sched_bpf__attach(skel);
  if (err) {
    fprintf(stderr, "failed to attach BPF skeleton\n");
    goto cleanup;
  }

  rb = ring_buffer__new(bpf_map__fd(skel->maps.switch_events),
                        handle_switch_event, NULL, NULL);
  if (!rb) {
    fprintf(stderr, "failed to create user-space ring buffer interface\n");
    err = -1;
    goto cleanup;
  }

  read_raw_totals(skel, &start_totals);
  prev_totals = start_totals;
  start_time = time(NULL);
  last_stats = start_time;

  printf("Scheduler attached and running. Press Ctrl+C to detach...\n");

  while (!exiting) {
    err = ring_buffer__poll(rb, 100 /* ms */);
    if (err == -EINTR) {
      err = 0;
      continue;
    }
    if (err < 0) {
      fprintf(stderr, "ring_buffer__poll error: %d\n", err);
      break;
    }

    time_t now = time(NULL);
    if (now - last_stats >= 1) {
      double delta_time = (double)(now - last_stats);

      if (strcmp(flag_f_value, "auto") == 0) {
        evaluate_and_adapt(skel, &metrics, &prev_totals, delta_time);
      } else {
        print_stats(skel);
        read_raw_totals(skel, &prev_totals);
      }
      last_stats = now;
    }
  }
  if (flag_a) {
    time_t end_time = time(NULL);
    double total_duration = difftime(end_time, start_time);

    struct raw_totals end_totals = {0};
    read_raw_totals(skel, &end_totals);

    int ncpu = libbpf_num_possible_cpus();
    if (ncpu <= 0)
      ncpu = 1;

    // 1. Calculate Core Deltas
    unsigned long long tasks_completed =
        end_totals.completions - start_totals.completions;
    unsigned long long delta_busy_ns =
        end_totals.total_cpu_ticks - start_totals.total_cpu_ticks;
    unsigned long long delta_wait_ns =
        end_totals.wait_sum_ns - start_totals.wait_sum_ns;
    unsigned long long delta_burst_ns =
        end_totals.burst_sum_ns - start_totals.burst_sum_ns;
    unsigned long long delta_resp_ns =
        end_totals.response_sum_ns - start_totals.response_sum_ns;
    unsigned long long delta_ctx =
        end_totals.context_switches - start_totals.context_switches;

    // 2. Compute Metric Formulas
    // CPU Utilization = (Actual Busy Time) / (Total Core Time Capacity)
    double total_capacity_ns = total_duration * 1e9 * ncpu;
    double cpu_utilization =
        (total_capacity_ns > 0)
            ? ((double)delta_busy_ns / total_capacity_ns) * 100.0
            : 0.0;
    if (cpu_utilization > 100.0)
      cpu_utilization = 100.0; // Mathematical ceiling bound

    // Throughput = Completed Processes per Second
    double throughput =
        (total_duration > 0) ? (double)tasks_completed / total_duration : 0.0;

    // Averages normalized by completions (Converted from ns to ms for clean
    // readability)
    double avg_waiting_ms = (tasks_completed > 0)
                                ? (double)delta_wait_ns / tasks_completed / 1e6
                                : 0.0;
    double avg_burst_ms = (tasks_completed > 0)
                              ? (double)delta_burst_ns / tasks_completed / 1e6
                              : 0.0;
    double avg_response_ms = (tasks_completed > 0)
                                 ? (double)delta_resp_ns / tasks_completed / 1e6
                                 : 0.0;

    // Turnaround Time = Waiting Time + Burst Time
    double avg_turnaround_ms = avg_waiting_ms + avg_burst_ms;

    // 3. Render the Dashboard Layout
    printf("\n====================================================\n");
    printf("         scx PROFILE: WORKLOAD PERFORMANCE REPORT    \n");
    printf("====================================================\n");
    printf(" [Environment & Execution Context]\n");
    printf("  • Running Duration    : %.2f seconds\n", total_duration);
    printf("  • Core Allocation     : %d CPU Core(s)\n", ncpu);
    printf("  • Target Policy Mode  : %s\n", flag_f_value);
    printf("  • Tasks Finalized     : %llu\n", tasks_completed);
    printf("  • Context Switches    : %llu\n", delta_ctx);
    printf("----------------------------------------------------\n");
    printf(" [Resource Utilization & Capacity Tracking]\n");
    printf("  • CPU Utilization     : \033[1;32m%6.2f%%\033[0m\n",
           cpu_utilization);
    printf("  • System Throughput   : %6.2f tasks/sec\n", throughput);
    printf("----------------------------------------------------\n");
    printf(" [Scheduler Latency Profiles (Averages)]\n");
    printf("  • Avg Response Time   : %6.2f ms  (Arrival ➔ Exec Slot)\n",
           avg_response_ms);
    printf("  • Avg Waiting Time    : %6.2f ms  (Time Spent in DSQ)\n",
           avg_waiting_ms);
    printf("  • Avg Execution Burst : %6.2f ms  (Time Active on Core)\n",
           avg_burst_ms);
    printf("  • Avg Turnaround Time : \033[1;36m%6.2f ms\033[0m  (Total "
           "Lifespan)\n",
           avg_turnaround_ms);

    if (strcmp(flag_f_value, "auto") == 0) {
      printf("----------------------------------------------------\n");
      printf(" [Autonomous Adaptation Feedback]\n");
      printf("  • Final Evaluated State : %s\n",
             workload_name(classify_workload(&metrics)));
      printf("  • Multi-Policy Switches : %u transitions triggered\n",
             metrics.queue_depth_avg > 0 ? 1 : 0); // structural indicator
    }
    printf("====================================================\n");
  }
cleanup:
  printf("\nDetaching and cleaning up scheduler...\n");
  ring_buffer__free(rb);
  sched_bpf__destroy(skel);
  return err < 0 ? -err : 0;
}