CLANG      := clang
CC         := gcc
BPFTOOL    := bpftool
BPF_CFLAGS := -g -O2 -target bpf -Wall -D__x86_64__ -Isrc

CFLAGS     := -Wall -O2 -Ibuild -I. -I ./src/include
LDFLAGS    := -lbpf -lelf -lz

APP        := sched
SRC_DIR    := src
BUILD_DIR  := build
HEADERS_DIR:= src/headers

VMLINUX    := $(HEADERS_DIR)/vmlinux.h
BPF_SKEL   := $(HEADERS_DIR)/$(APP).skel.h
BPF_OBJ    := $(BUILD_DIR)/$(APP).bpf.o
USER_OBJ   := $(BUILD_DIR)/$(APP).o
TARGET     := $(BUILD_DIR)/$(APP)

.PHONY: all clean

all: $(TARGET)

$(VMLINUX):
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

$(BPF_OBJ): $(SRC_DIR)/$(APP).bpf.c $(VMLINUX)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

$(BPF_SKEL): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > $@

$(USER_OBJ): $(SRC_DIR)/$(APP).c $(BPF_SKEL)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(USER_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(BPF_OBJ) $(BPF_SKEL) $(USER_OBJ) $(TARGET)
	@echo "Kept vmlinux.h to save generation time. Use 'rm $(VMLINUX)' to wipe completely."
