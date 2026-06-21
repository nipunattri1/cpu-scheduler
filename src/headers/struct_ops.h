#pragma once
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define BPF_STRUCT_OPS(name, args...)                                          \
  SEC("struct_ops/" #name)                                                     \
  BPF_PROG(name, ##args)

#define BPF_STRUCT_OPS_SLEEPABLE(name, args...)                                \
  SEC("struct_ops.s/" #name)                                                   \
  BPF_PROG(name, ##args)