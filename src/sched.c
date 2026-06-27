#include "headers/adapt.h"
#include "headers/helpers.h"
#include "headers/sched.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t exiting = 0;
static void sig_handler(int sig) { exiting = 1; }

int main(void) {
  struct sched_bpf *skel;
  struct ring_buffer *rb = NULL;
  time_t last_stats;
  int err = 0;

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

  printf("Scheduler attached. Logging policy switches and stats...\n");

  struct workload_metrics metrics = {0};
  struct raw_totals prev_totals = {0};
  read_raw_totals(skel, &prev_totals); /* baseline */

  last_stats = time(NULL);
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
      // print_stats(skel);
      evaluate_and_adapt(skel, &metrics, &prev_totals,
                         (double)(now - last_stats));
      last_stats = now;
    }
  }

cleanup:
  printf("\nDetaching and cleaning up scheduler...\n");
  ring_buffer__free(rb);
  sched_bpf__destroy(skel);
  return err < 0 ? -err : 0;
}