#include "headers/sched.skel.h"
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t exiting = 0;

static void sig_handler(int sig) { exiting = 1; }

int main(void) {
  struct sched_bpf *skel;
  int err;

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
cleanup:
  sched_bpf__destroy(skel);
  return err < 0 ? -err : 0;
}