CLANG      := clang
CC         := gcc
BPFTOOL    := bpftool
BPF_CFLAGS := -g -O2 -target bpf -Wall -D__x86_64__ -Isrc -Isrc/headers

CFLAGS     := -Wall -O2 -Ibuild -I. -I ./src/headers
LDFLAGS    := -lbpf -lelf -lz -lm

APP        := sched
SRC_DIR    := src
BUILD_DIR  := build
HEADERS_DIR:= src/headers

VMLINUX    := $(HEADERS_DIR)/vmlinux.h
BPF_SKEL   := $(HEADERS_DIR)/$(APP).skel.h
BPF_OBJ    := $(BUILD_DIR)/$(APP).bpf.o

# Fix: Pluralize and explicitly include adapt.o alongside sched.o
USER_OBJS  := $(BUILD_DIR)/$(APP).o $(BUILD_DIR)/adapt.o
TARGET     := $(BUILD_DIR)/$(APP)

.PHONY: all clean

all: $(TARGET)

$(VMLINUX):
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

$(BPF_OBJ): $(SRC_DIR)/$(APP).bpf.c $(VMLINUX)
	@mkdir -p $(BUILD_DIR)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

$(BPF_SKEL): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > $@

# Fix: Use a pattern rule to automatically compile any .c file in src/ into build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BPF_SKEL)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Fix: Link all compiled object files into the final executable
$(TARGET): $(USER_OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(BPF_OBJ) $(BPF_SKEL) $(USER_OBJS) $(TARGET)
	@echo "Kept vmlinux.h to save generation time. Use 'rm $(VMLINUX)' to wipe completely."