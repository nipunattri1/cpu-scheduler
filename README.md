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
bin/schbench

```

*Expected Behaviour:* The controller log interface (especially when accompanied by the `-a` flag) will document a spike in telemetry metrics—notably context switching frequencies and queue latencies—prompting the internal hysteresis-gated feedback loop to dynamically adapt execution rings to handle the heavy lock contention.

---

## Stats

```md
okiw, i got some stats
         auto:
          
            min=4025, max=101210                                                                                                                                                                                                                                                           [0/13]
RPS percentiles (requests) runtime 10 (s) (11 total samples)
          20.0th: 1862       (3 samples)
        * 50.0th: 1874       (3 samples)
          90.0th: 1910       (4 samples)
          min=1851, max=1934
sched delay: message 8 (usec) worker 1 (usec)
current rps: 1851.21
Wakeup Latencies percentiles (usec) runtime 20 (s) (37019 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (9414 samples)
        * 99.0th: 28         (2046 samples)
          99.9th: 527        (321 samples)
          min=1, max=11732
Request Latencies percentiles (usec) runtime 20 (s) (37029 total samples)
          50.0th: 8496       (10981 samples)
          90.0th: 8976       (14945 samples)
        * 99.0th: 11824      (3117 samples)
          99.9th: 21792      (332 samples)
          min=4025, max=101210
RPS percentiles (requests) runtime 20 (s) (21 total samples)
          20.0th: 1806       (6 samples)
        * 50.0th: 1834       (5 samples)
          90.0th: 1906       (8 samples)
          min=1799, max=1934
sched delay: message 7 (usec) worker 1 (usec)
current rps: 1799.10
Wakeup Latencies percentiles (usec) runtime 30 (s) (54492 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (14602 samples)
        * 99.0th: 31         (3390 samples)
          99.9th: 689        (479 samples)
          min=1, max=11732
Request Latencies percentiles (usec) runtime 30 (s) (54516 total samples)
          50.0th: 8688       (20179 samples)
          90.0th: 9168       (16520 samples)
        * 99.0th: 12624      (4761 samples)
          99.9th: 22816      (486 samples)
          min=4025, max=101210
RPS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1762       (7 samples)
        * 50.0th: 1806       (9 samples)
          90.0th: 1898       (12 samples)
          min=1656, max=1934
sched delay: message 8 (usec) worker 1 (usec)
current rps: 1744.35
Wakeup Latencies percentiles (usec) runtime 30 (s) (54493 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (14602 samples)
        * 99.0th: 31         (3391 samples)
          99.9th: 689        (479 samples)
          min=1, max=11732
Request Latencies percentiles (usec) runtime 30 (s) (54532 total samples)
          50.0th: 8688       (20180 samples)
          90.0th: 9168       (16529 samples)
        * 99.0th: 12624      (4762 samples)
          99.9th: 22816      (487 samples)
          min=4025, max=101210
RPS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1762       (7 samples)
        * 50.0th: 1806       (9 samples)
          90.0th: 1898       (12 samples)
          min=1656, max=1934
average rps: 1817.73
sched delay: message 0 (usec) worker 0 (usec)
fcfs:
13]       90.0th: 1922       (4 samples)                                                                                                                                                                                                                                                 [0/13]       min=1788, max=1933
sched delay: message 14 (usec) worker 3 (usec)
current rps: 1802.83
Wakeup Latencies percentiles (usec) runtime 20 (s) (36484 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (8597 samples)
        * 99.0th: 57         (2688 samples)
          99.9th: 2060       (323 samples)
          min=1, max=6475
Request Latencies percentiles (usec) runtime 20 (s) (36501 total samples)
          50.0th: 8592       (11550 samples)
          90.0th: 9072       (14101 samples)
        * 99.0th: 15440      (3176 samples)
          99.9th: 24480      (329 samples)
          min=3858, max=89683
RPS percentiles (requests) runtime 20 (s) (21 total samples)
          20.0th: 1774       (5 samples)
        * 50.0th: 1810       (6 samples)
          90.0th: 1906       (8 samples)
          min=1744, max=1933
sched delay: message 11 (usec) worker 2 (usec)
current rps: 1773.87
Wakeup Latencies percentiles (usec) runtime 30 (s) (53569 total samples)
          50.0th: 4          (0 samples)
          90.0th: 10         (13864 samples)
        * 99.0th: 69         (3775 samples)
          99.9th: 2660       (475 samples)
          min=1, max=6475
Request Latencies percentiles (usec) runtime 30 (s) (53601 total samples)
          50.0th: 8688       (17749 samples)
          90.0th: 9360       (18962 samples)
        * 99.0th: 16272      (4590 samples)
          99.9th: 28320      (479 samples)
          min=3858, max=97518
RPS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1722       (7 samples)
        * 50.0th: 1790       (10 samples)
          90.0th: 1874       (11 samples)
          min=1637, max=1933
sched delay: message 15 (usec) worker 3 (usec)
current rps: 1637.88
Wakeup Latencies percentiles (usec) runtime 30 (s) (53569 total samples)
          50.0th: 4          (0 samples)
          90.0th: 10         (13864 samples)
        * 99.0th: 69         (3775 samples)
          99.9th: 2660       (475 samples)
          min=1, max=6475
Request Latencies percentiles (usec) runtime 30 (s) (53617 total samples)
          50.0th: 8688       (17750 samples)
          90.0th: 9360       (18970 samples)

          50.0th: 8688       (17750 samples)
          90.0th: 9360       (18970 samples)
        * 99.0th: 16272      (4594 samples)
          99.9th: 28320      (479 samples)
          min=3858, max=97518
 PS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1722       (7 samples)
        * 50.0th: 1790       (10 samples)
          90.0th: 1874       (11 samples)
          min=1637, max=1933
 verage rps: 1787.23
sched delay: message 0 (usec) worker 0 (usec)
priority:
3]     * 50.0th: 1878       (3 samples)                                                                                                                                                                                                                                                 [0/13]       90.0th: 1926       (4 samples)
          min=1831, max=1955
sched delay: message 7 (usec) worker 1 (usec)
current rps: 1831.01
Wakeup Latencies percentiles (usec) runtime 20 (s) (36272 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (8990 samples)
        * 99.0th: 74         (3107 samples)
          99.9th: 2988       (329 samples)
          min=1, max=19689
Request Latencies percentiles (usec) runtime 20 (s) (36294 total samples)
          50.0th: 8528       (10685 samples)
          90.0th: 9104       (14373 samples)
        * 99.0th: 16992      (3223 samples)
          99.9th: 23840      (324 samples)
          min=4097, max=102279
RPS percentiles (requests) runtime 20 (s) (21 total samples)
          20.0th: 1734       (5 samples)
        * 50.0th: 1814       (6 samples)
          90.0th: 1906       (8 samples)
          min=1577, max=1955
sched delay: message 16 (usec) worker 5 (usec)
current rps: 1577.18
Wakeup Latencies percentiles (usec) runtime 30 (s) (53690 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (13962 samples)
        * 99.0th: 70         (4763 samples)
          99.9th: 2988       (485 samples)
          min=1, max=19689
Request Latencies percentiles (usec) runtime 30 (s) (53725 total samples)
          50.0th: 8688       (17876 samples)
          90.0th: 9264       (18759 samples)
        * 99.0th: 16672      (4759 samples)
          99.9th: 24032      (481 samples)
          min=4097, max=102279
RPS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1730       (7 samples)
        * 50.0th: 1762       (9 samples)
          90.0th: 1890       (12 samples)
          min=1577, max=1955
sched delay: message 16 (usec) worker 4 (usec)
current rps: 1744.88
Wakeup Latencies percentiles (usec) runtime 30 (s) (53691 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (13962 samples)
        * 99.0th: 70         (4764 samples)
          99.9th: 2988       (485 samples)
          min=1, max=19689
Request Latencies percentiles (usec) runtime 30 (s) (53742 total samples)
          50.0th: 8688       (17877 samples)
          90.0th: 9264       (18768 samples)
          50.0th: 8688       (17877 samples)
          90.0th: 9264       (18768 samples)
        * 99.0th: 16672      (4759 samples)
          99.9th: 24352      (482 samples)
          min=4097, max=102279
 PS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1730       (7 samples)
        * 50.0th: 1762       (9 samples)
          90.0th: 1890       (12 samples)
          min=1577, max=1955
 verage rps: 1791.40
sched delay: message 0 (usec) worker 0 (usec)

rr:
13]     * 50.0th: 1834       (3 samples)                                                                                                                                                                                                                                                 [0/13]       90.0th: 1878       (4 samples)
          min=1726, max=1946
sched delay: message 11 (usec) worker 2 (usec)
current rps: 1726.61
Wakeup Latencies percentiles (usec) runtime 20 (s) (36154 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (8562 samples)
        * 99.0th: 46         (2436 samples)
          99.9th: 1206       (320 samples)
          min=1, max=3823
Request Latencies percentiles (usec) runtime 20 (s) (36165 total samples)
          50.0th: 8688       (12075 samples)
          90.0th: 9168       (11913 samples)
        * 99.0th: 13968      (3057 samples)
          99.9th: 23584      (325 samples)
          min=3750, max=45146
RPS percentiles (requests) runtime 20 (s) (21 total samples)
          20.0th: 1758       (5 samples)
        * 50.0th: 1798       (6 samples)
          90.0th: 1874       (8 samples)
          min=1726, max=1946
sched delay: message 11 (usec) worker 1 (usec)
current rps: 1764.77
Wakeup Latencies percentiles (usec) runtime 30 (s) (53535 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (13017 samples)
        * 99.0th: 41         (3575 samples)
          99.9th: 1003       (476 samples)
          min=1, max=3823
Request Latencies percentiles (usec) runtime 30 (s) (53560 total samples)
          50.0th: 8784       (16115 samples)
          90.0th: 9296       (20848 samples)
        * 99.0th: 13456      (4761 samples)
          99.9th: 23648      (483 samples)
          min=3750, max=45146
RPS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1738       (7 samples)
        * 50.0th: 1762       (9 samples)
          90.0th: 1870       (12 samples)
          min=1710, max=1946
sched delay: message 10 (usec) worker 1 (usec)
current rps: 1724.65
Wakeup Latencies percentiles (usec) runtime 30 (s) (53538 total samples)
          50.0th: 4          (0 samples)
          90.0th: 9          (13018 samples)
        * 99.0th: 41         (3577 samples)
          99.9th: 1003       (476 samples)
          min=1, max=3823
Request Latencies percentiles (usec) runtime 30 (s) (53578 total samples)
          50.0th: 8784       (16117 samples)
          90.0th: 9296       (20857 samples)
          50.0th: 8784       (16117 samples)
          90.0th: 9296       (20857 samples)
        * 99.0th: 13488      (4767 samples)
          99.9th: 23648      (481 samples)
          min=3750, max=45146
 PS percentiles (requests) runtime 30 (s) (31 total samples)
          20.0th: 1738       (7 samples)
        * 50.0th: 1762       (9 samples)
          90.0th: 1870       (12 samples)
          min=1710, max=1946
 verage rps: 1785.93
sched delay: message 0 (usec) worker 0 (usec)
(sorry for the little unformatted code), you can refer to this gemini analysis of the above stats if you wish to, https://share.gemini.google/9uOr4K9dvnaw
```

---

## ⚖ License

This software framework is released under the **GPL-2.0 ONLY** license matching standard core subsystem rules governing Linux kernel development extension utilities.
