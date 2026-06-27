#ifndef SCHED_ADAPT
#define SCHED_ADAPT
#include "helpers.h"

 void evaluate_and_adapt(struct sched_bpf *skel,
                               struct workload_metrics *m,
                               struct raw_totals *prev, double dt_sec);

 int handle_switch_event(void *ctx, void *data, size_t len);
#endif