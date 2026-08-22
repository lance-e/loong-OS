.DEFAULT_GOAL := help

.PHONY : help

help:
	@echo "准备工具： make prepare"
	@echo "编译镜像： make build"
	@echo "写入镜像： make hd"
	@echo "完整执行： make all"

LOONG_ROOT := $(realpath $(dir $(firstword $(MAKEFILE_LIST))))
TOOLCHAIN_ROOT := $(LOONG_ROOT)/.toolchains
TOOLCHAIN_BIN := $(TOOLCHAIN_ROOT)/bin
TOOLCHAIN_SRC := $(TOOLCHAIN_ROOT)/src
TOOLCHAIN_OPT := $(TOOLCHAIN_ROOT)/opt

GCC34_ROOT := $(TOOLCHAIN_OPT)/gcc-3.4
GCC34_EXEC_DIR := $(GCC34_ROOT)/usr/lib/gcc/x86_64-linux-gnu/3.4.6
NASM_PREFIX := $(TOOLCHAIN_OPT)/nasm-2.15.05
BOCHS_PREFIX := $(TOOLCHAIN_OPT)/bochs-2.6.2

GCC_BASE_DEB := $(LOONG_ROOT)/gcc3.4lib/gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb
GCC_CPP_DEB := $(LOONG_ROOT)/gcc3.4lib/cpp-3.4_3.4.6-6ubuntu3_amd64.deb
GCC_DEB := $(LOONG_ROOT)/gcc3.4lib/gcc-3.4_3.4.6-6ubuntu3_amd64.deb
GCC_DEBS := $(GCC_BASE_DEB) $(GCC_CPP_DEB) $(GCC_DEB)
NASM_ARCHIVE := $(LOONG_ROOT)/tool/nasm-2.15.05.tar.gz
BOCHS_ARCHIVE := $(LOONG_ROOT)/tool/bochs-2.6.2.tar.gz
NASM_SOURCE := $(TOOLCHAIN_SRC)/nasm-2.15.05
BOCHS_SOURCE := $(TOOLCHAIN_SRC)/bochs-2.6.2

BUILD_DIR = $(LOONG_ROOT)/build
ENTRY_POINT = 0xc0001500
AS := $(TOOLCHAIN_BIN)/nasm
CC := env \
	  GCC_EXEC_PREFIX="$(GCC34_ROOT)/usr/lib/gcc/" \
	  COMPILER_PATH="$(GCC34_EXEC_DIR)" \
	  $(TOOLCHAIN_BIN)/gcc 
LD = ld
BOCHS := $(TOOLCHAIN_BIN)/bochs
# 编译 NASM 和 Bochs 时使用宿主机编译器
HOST_CC ?= /usr/bin/gcc
HOST_CXX ?= /usr/bin/g++
JOBS ?= $(shell nproc 2>/dev/null || echo 1)

export LOONG_ROOT
export BXSHARE := $(BOCHS_PREFIX)/share/bochs


LIB = -I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/	\
      -I shell/
ASFLAGS = -f elf
CFLAGS = -m32 -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main 
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o 	\
	$(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o	\
	$(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o	\
	$(BUILD_DIR)/switch.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o	\
	$(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o		\
	$(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/file.o $(BUILD_DIR)/inode.o		\
	$(BUILD_DIR)/dir.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/buildin_cmd.o	\
	$(BUILD_DIR)/exec.o $(BUILD_DIR)/assert.o



############   compile  C     ###########

$(BUILD_DIR)/main.o : kernel/main.c lib/kernel/print.h	\
	lib/stdint.h  kernel/init.h kernel/memory.h  thread/thread.h	\
	kernel/interrupt.h device/console.h device/ioqueue.h 	\
	device/keyboard.h userprog/process.h fs/fs.h lib/string.h	\
	fs/dir.h shell/shell.h kernel/debug.h lib/kernel/stdio-kernel.h	\
	device/ide.h
	$(CC) $(CFLAGS)  $< -o $@

$(BUILD_DIR)/init.o : kernel/init.c kernel/init.h lib/kernel/print.h \
	lib/stdint.h kernel/interrupt.h device/timer.h thread/thread.h	\
	device/console.h device/keyboard.h userprog/tss.h 	\
	userprog/syscall-init.h device/ide.h fs/fs.h userprog/tss.h
	$(CC) $(CFLAGS) $<  -o $@

$(BUILD_DIR)/interrupt.o : kernel/interrupt.c kernel/interrupt.h	\
	lib/stdint.h kernel/global.h lib/kernel/io.h lib/kernel/print.h	\
	kernel/debug.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o : device/timer.c device/timer.h lib/stdint.h	\
	lib/kernel/io.h lib/kernel/print.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o : kernel/debug.c kernel/debug.h 	\
	lib/kernel/print.h lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o : lib/string.c lib/string.h 	\
	lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o : lib/kernel/bitmap.c lib/kernel/bitmap.h	\
	kernel/global.h lib/stdint.h lib/string.h lib/kernel/print.h	\
	kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o : kernel/memory.c kernel/memory.h	\
	lib/kernel/print.h lib/stdint.h lib/kernel/bitmap.h	\
	kernel/global.h lib/string.h kernel/debug.h thread/sync.h	\
	thread/thread.h	 kernel/interrupt.h lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o : thread/thread.c thread/thread.h 	\
	lib/stdint.h lib/string.h kernel/global.h 	\
	kernel/memory.h lib/kernel/list.h kernel/interrupt.h	\
	kernel/debug.h lib/kernel/print.h lib/kernel/list.h	\
	kernel/memory.h  userprog/process.h thread/sync.h	\
	lib/stdio.h fs/fs.h fs/file.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o : lib/kernel/list.c lib/kernel/list.h	\
	kernel/global.h kernel/interrupt.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o : thread/sync.c thread/sync.h lib/stdint.h	\
	lib/kernel/list.h thread/thread.h kernel/debug.h kernel/interrupt.h	
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o : device/console.c device/console.h 	\
	lib/kernel/print.h lib/stdint.h thread/sync.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o : device/keyboard.c device/keyboard.h	\
	lib/kernel/print.h kernel/interrupt.h kernel/global.h	\
	lib/kernel/io.h device/ioqueue.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o : device/ioqueue.c device/ioqueue.h	\
	kernel/interrupt.h kernel/global.h kernel/debug.h	\
	thread/thread.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o : userprog/tss.c userprog/tss.h		\
	thread/thread.h kernel/global.h lib/stdint.h lib/kernel/print.h	\
	lib/string.h	
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o : userprog/process.c userprog/process.h	\
	thread/thread.h kernel/global.h lib/kernel/print.h	\
	lib/kernel/list.h kernel/global.h lib/string.h 		\
	kernel/interrupt.h userprog/tss.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o : lib/user/syscall.c lib/user/syscall.h 	\
	lib/stdint.h thread/thread.h fs/dir.h fs/fs.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o : userprog/syscall-init.c		\
	userprog/syscall-init.h thread/thread.h lib/kernel/print.h	\
	lib/stdint.h lib/user/syscall.h lib/string.h		\
	fs/fs.h userprog/fork.h device/console.h userprog/exec.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o : lib/stdio.c lib/stdio.h  lib/user/syscall.h	\
	lib/stdint.h lib/string.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio-kernel.o: lib/kernel/stdio-kernel.c 	lib/kernel/stdio-kernel.h	\
	lib/stdio.h device/console.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h lib/stdint.h lib/kernel/list.h	\
	lib/kernel/bitmap.h thread/sync.h kernel/global.h kernel/debug.h 	\
	lib/kernel/stdio-kernel.h device/timer.h lib/stdio.h kernel/memory.h	\
	lib/kernel/io.h lib/string.h kernel/interrupt.h fs/super_block.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fs.o: fs/fs.c fs/fs.h kernel/global.h fs/super_block.h fs/dir.h 	\
	lib/kernel/stdio-kernel.h kernel/memory.h kernel/debug.h device/ide.h	\
	lib/stdint.h lib/string.h fs/file.h thread/thread.h device/console.h	\
	device/ioqueue.h device/keyboard.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o: fs/file.c fs/file.h lib/stdint.h device/ide.h fs/fs.h 		\
	lib/kernel/stdio-kernel.h thread/thread.h lib/kernel/bitmap.h fs/super_block.h	\
	fs/dir.h kernel/memory.h fs/inode.h lib/string.h fs/dir.h kernel/interrupt.h	\
	kernel/global.h kernel/debug.h device/ioqueue.h device/keyboard.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c fs/dir.h lib/stdint.h fs/inode.h fs/fs.h kernel/global.h	\
	kernel/memory.h lib/kernel/stdio-kernel.h  device/ide.h fs/super_block.h	\
	fs/inode.h  kernel/debug.h fs/file.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o: fs/inode.c fs/inode.h lib/stdint.h lib/kernel/list.h 	\
	kernel/global.h device/ide.h fs/super_block.h lib/string.h 	\
	kernel/interrupt.h fs/file.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c userprog/fork.h lib/stdint.h	\
	thread/thread.h kernel/memory.h lib/string.h kernel/global.h	\
	userprog/process.h lib/kernel/bitmap.h lib/kernel/list.h 	\
	kernel/interrupt.h kernel/debug.h fs/file.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/shell.o: shell/shell.c shell/shell.h lib/stdint.h 		\
	lib/stdio.h lib/user/assert.h lib/user/syscall.h lib/string.h	\
	fs/file.h fs/fs.h shell/buildin_cmd.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/buildin_cmd.o: shell/buildin_cmd.c shell/buildin_cmd.h 	\
	lib/stdint.h lib/string.h fs/fs.h fs/dir.h lib/user/assert.h	\
	lib/user/syscall.h kernel/global.h lib/stdio.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/exec.o: userprog/exec.c userprog/exec.h lib/stdint.h 	\
	kernel/memory.h kernel/interrupt.h fs/fs.h lib/string.h		\
	thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/assert.o : lib/user/assert.c lib/user/assert.h lib/stdio.h	
	$(CC) $(CFLAGS) $< -o $@




############    compile asm   	     ##############

$(BUILD_DIR)/kernel.o : kernel/kernel.S		
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o : lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o : thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

############ 	lind all object file ##############
$(BUILD_DIR)/kernel.bin : $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.PHONY: mk_dir

mk_dir:
	if [ ! -d $(BUILD_DIR)  ]; then mkdir $(BUILD_DIR);fi

.PHONY: prepare-gcc

prepare-gcc:
	@set -eu; \
	echo "[prepare] 重新准备 GCC 3.4"; \
	mkdir -p "$(TOOLCHAIN_BIN)" "$(TOOLCHAIN_OPT)"; \
	rm -rf "$(GCC34_ROOT)"; \
	mkdir -p "$(GCC34_ROOT)"; \
	for deb in $(GCC_DEBS); do \
			ar p "$$deb" data.tar.gz | tar -xzf - -C "$(GCC34_ROOT)"; \
	done; \
	ln -sfn ../opt/gcc-3.4/usr/bin/gcc-3.4 "$(TOOLCHAIN_BIN)/gcc"; \
	ln -sfn ../opt/gcc-3.4/usr/bin/cpp-3.4 "$(TOOLCHAIN_BIN)/cpp"; \
	test -x "$(TOOLCHAIN_BIN)/gcc"; \
	test -x "$(GCC34_ROOT)/usr/lib/gcc/x86_64-linux-gnu/3.4.6/cc1"

.PHONY: prepare-nasm

prepare-nasm:
	@set -eu; \
	echo "[prepare] 重新编译 NASM 2.15.05"; \
	mkdir -p "$(TOOLCHAIN_BIN)" "$(TOOLCHAIN_SRC)" "$(TOOLCHAIN_OPT)"; \
	rm -rf "$(NASM_SOURCE)" "$(NASM_PREFIX)"; \
	tar -xzf "$(NASM_ARCHIVE)" -C "$(TOOLCHAIN_SRC)"; \
	cd "$(NASM_SOURCE)" && \
			CC="$(HOST_CC)" ./configure --prefix="$(NASM_PREFIX)"; \
	$(MAKE) -C "$(NASM_SOURCE)" -j"$(JOBS)"; \
	$(MAKE) -C "$(NASM_SOURCE)" install; \
	ln -sfn ../opt/nasm-2.15.05/bin/nasm "$(TOOLCHAIN_BIN)/nasm"; \
	test -x "$(TOOLCHAIN_BIN)/nasm"


.PHONY: prepare-bochs

prepare-bochs:
	@set -eu; \
	echo "[prepare] 重新编译 Bochs 2.6.2"; \
	mkdir -p "$(TOOLCHAIN_BIN)" "$(TOOLCHAIN_SRC)" "$(TOOLCHAIN_OPT)"; \
	rm -rf "$(BOCHS_SOURCE)" "$(BOCHS_PREFIX)"; \
	tar -xzf "$(BOCHS_ARCHIVE)" -C "$(TOOLCHAIN_SRC)"; \
	cd "$(BOCHS_SOURCE)" && \
			CC="$(HOST_CC)" CXX="$(HOST_CXX)" ./configure \
					--prefix="$(BOCHS_PREFIX)" \
					--enable-debugger \
					--enable-disasm \
					--enable-iodebug \
					--enable-x86-debugger \
					--with-x \
					--with-x11 \
					--disable-plugins; \
	$(MAKE) -C "$(BOCHS_SOURCE)" -j"$(JOBS)"; \
	$(MAKE) -C "$(BOCHS_SOURCE)" install; \
	ln -sfn ../opt/bochs-2.6.2/bin/bochs "$(TOOLCHAIN_BIN)/bochs"; \
	ln -sfn ../opt/bochs-2.6.2/bin/bximage "$(TOOLCHAIN_BIN)/bximage"; \
	ln -sfn ../opt/bochs-2.6.2/bin/bxcommit "$(TOOLCHAIN_BIN)/bxcommit"; \
	test -x "$(TOOLCHAIN_BIN)/bochs"; \
	test -f "$(BXSHARE)/BIOS-bochs-latest"; \
	test -f "$(BXSHARE)/VGABIOS-lgpl-latest"


.PHONY: prepare

prepare: mk_dir prepare-gcc prepare-nasm prepare-bochs
	@echo "[prepare] 工具链重新编译完成"


.PHONY : build hd clean all

hd:
	dd if=$(BUILD_DIR)/kernel.bin 	\
	   of=./image/hd60M.img		\
	   bs=512 count=200 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -r ./*

build: $(BUILD_DIR)/kernel.bin

all: prepare build hd

.PHONY : run 

run: 
	"$(BOCHS)" 
