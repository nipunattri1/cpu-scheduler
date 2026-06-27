# scx_adaptive: Closed-Loop Multi-Policy eBPF Scheduler

`scx_adaptive` is an adaptive, dynamic kernel-level CPU scheduling framework powered by **`sched_ext` (SCX)** and **eBPF**. By marrying the low overhead of in-kernel scheduling rings with user-space runtime classification telemetry, the system dynamically changes scheduling policies on-the-fly to match evolving real-world workloads.

The engine profiles CPU utilization, task burst execution lengths, and queue latency profiles, choosing between **First-Come-First-Served (FCFS)**, **Round-Robin (RR)**, and **Priority-based** queues via a hysteresis-gated feedback loop.

---

## 🚀 Key Features

* **Pluggable Multi-Policy Core:** Contains individual kernel-space schedulers (`FCFS`, `Round Robin`, and `Static Priority`) modularly compiled via eBPF.
* **Closed-Loop Adaptivity:** User-space control routines evaluate raw system metrics exported via BPF maps to determine workload patterns in real-time.
* **Intelligent Workload Classification:** Uses Exponential Moving Averages (EMA) to map tasks to distinct profiles (`CPU_BOUND`, `INTERACTIVE`, `BATCH`, or `STRESS`).
* **Hysteresis Protection:** Mitigates policy thrashing by employing confidence-gated barriers, filtering out transient spikes before altering execution topologies.
* **Zero Downtime Resilience:** Gracefully safely detaches via `SIGINT`/`SIGTERM` handlers, turning control back over to native Linux scheduling subsystems (e.g., EEVDF).

---

## 🏛 Architecture Overview

```text
  +------------------------------------------------------------+
  |                        USER-SPACE                          |
  |                                                            |
  |     +-------------------------+     Polled Metrics         |
  |     |   evaluate_and_adapt()  | <====================+     |
  |     +------------+------------+                      ║     |
  |                  |                                   ║     |
  |                  | Calculates Scores                 ║     |
  |                  v                                   ║     |
  |     +-------------------------+                      ║     |
  |     |   target_policy_g       |                      ║     |
  |     +------------+------------+                      ║     |
  +------------------|-----------------------------------|-----+
                     | Modifies Global State             ║
  ===================|===================================|======
  +------------------v-----------------------------------|-----+
  |                     KERNEL-SPACE (eBPF)              ║     |
  |                                                      ║     |
  |      +------------------------+             +--------+---+ |
  |      |  sched_ext entry point |             | stats_map  | |
  |      +-----------+------------+             +------------+ |
  |                  |                               ^         |
  |                  | Dispatches via Policy Tag     | Logs    |
  |                  v                               | Metrics |
  |     +------------+------------+                  |         |
  |     |  FCFS  |   RR   | Priority  | -----------------+         |
  |     +-------------------------+                            |
  +------------------------------------------------------------+

```

---

## 📁 Repository Structure

```text
├── build/                  # Compiled object targets and generated user-space binary
├── makefile                # Automated multi-compiler build instruction pipeline
├── notes.md                # Development research tracking logs
├── README.md               # Infrastructure documentation
└── src/
    ├── adapt.c             # Policy recalculation, telemetry tracking and ring buffers
    ├── sched.c             # Main host initialization, signal trapping, and BPF lifetime management
    ├── sched.bpf.c         # Primary struct_ops execution engine for sched_ext
    ├── algos/              # Concrete scheduling algorithm implementations
    │   ├── fcfs.bpf.c
    │   ├── priority.bpf.c
    │   └── rr.bpf.c
    └── headers/            # Interface mapping declarations
        ├── adapt.h         # External adaptive loop access prototypes
        ├── enums.h         # System policy and DSQ identification IDs
        ├── helpers.h       # Mathematical calculation engines (EMA, classifiers)
        ├── log.h           # Unified schema structures for ring-buffers/statistics
        ├── maps.bpf.h      # Shared BPF maps (task_timing_map, stats_map)
        ├── struct_ops.h    # BPF tracing expansion helper macros
        └── vmlinux.h       # System kernel internal types (via bpftool)

```

---

## 🛠 Prerequisites

To compile and load this project, your environment must include:

1. **Kernel Support:** A modern Linux Kernel built with `CONFIG_BPF_SYSCALL` and native `sched_ext` patching (e.g., Ubuntu/Fedora kernels running SCX or custom configurations).
2. **Development Dependencies:**

* `clang` (>= v15.0) for targeting BPF compilation bytecode architectures.
* `gcc` for user-space control binary linkage assembler tools.
* `bpftool` installed locally to auto-generate kernel tracking skeletons.
* Standard shared system library development runtimes (`libbpf`, `libelf`, `zlib`).

---

## 💻 Compilation

The system relies on an automated dual-pass compilation pipeline via the root `makefile`. It handles dumping `vmlinux.h` metadata, compiling kernel-space BPF objects, auto-generating user-space access layouts, and linking standard system runtime environments:

```bash
# Clone and enter the repository
git clone <your-repo-link> scx_adaptive
cd scx_adaptive

# Build the complete architecture pipeline
make

```

To clean intermediate binary components while leaving the dumped system `vmlinux.h` interface untouched for build acceleration:

```bash
make clean

```

---

## 🔬 Testing and Evaluation

Running the scheduler within sandboxed, temporary testing frameworks like **`virtme-ng`** is highly recommended to isolate resource modifications safely from host machines.

### 1. Activating the System

Launch the generated user-space controller under standard superuser privileges. You can control execution and analysis via execution flags:

```bash
sudo build/sched [-a] [-f <policy>]

```

#### Available Flags

* **`-a`** : Enables analytical verbose printing to monitor detailed, real-time internal telemetry and metric tracking loops.
* **`-f <policy>`** : Forces or modifies the global scheduling mode. Valid policy choices include:
* `auto` (Default closed-loop dynamic selection)
* `fcfs`
* `rr`
* `priority`

> **Output Indicator:** `Scheduler attached. Logging policy switches and stats...`

### 2. Validating Runtime Adaptivity

Open an alternative interface terminal window while `scx_adaptive` runs. Execute the multi-threaded lock and yield stressor to challenge the active policy core:

```bash
sysbench threads --threads=64 --thread-yields=1000 --thread-locks=8 run

```

*Expected Behaviour:* The controller log interface (especially when accompanied by the `-a` flag) will document a spike in telemetry metrics—notably context switching frequencies and queue latencies—prompting the internal hysteresis-gated feedback loop to dynamically adapt execution rings to handle the heavy lock contention.

---

## Stats

```md
okiw, i got some stats
auto:
General statistics:
    total time:                          10.0343s
    total number of events:              30278

Latency (ms):
         min:                                    0.42
         avg:                                   21.18
         max:                                  136.98
         95th percentile:                       75.82
         sum:                               641180.11

Threads fairness:
    events (avg/stddev):           473.0938/27.57
    execution time (avg/stddev):   10.0184/0.01

rr:
General statistics:
    total time:                          10.0340s
    total number of events:              27979

Latency (ms):
         min:                                    0.40
         avg:                                   22.91
         max:                                  132.07
         95th percentile:                       74.46
         sum:                               641135.90

Threads fairness:
    events (avg/stddev):           437.1719/22.64
    execution time (avg/stddev):   10.0177/0.01

fcfs:
General statistics:
    total time:                          10.0309s
    total number of events:              26255

Latency (ms):
         min:                                    0.41
         avg:                                   24.42
         max:                                  136.59
         95th percentile:                       75.82
         sum:                               641075.13

Threads fairness:
    events (avg/stddev):           410.2344/25.36
    execution time (avg/stddev):   10.0168/0.01

Threads started!

priority:
General statistics:
    total time:                          10.0268s
    total number of events:              32629

Latency (ms):
         min:                                    0.43
         avg:                                   19.65
         max:                                  137.73
         95th percentile:                       84.47
         sum:                               641062.28

Threads fairness:
    events (avg/stddev):           509.8281/36.23
    execution time (avg/stddev):   10.0166/0.01
```

---

## ⚖ License

This software framework is released under the **GPL-2.0 ONLY** license matching standard core subsystem rules governing Linux kernel development extension utilities.
