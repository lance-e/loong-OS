#### this sh must run in command directory
#ld -e main $BIN".o" $OBJS -o $BIN
#SEC_CNT=$(ls -l $BIN/awk '{printf("%d",($5 + 511)/512)}')

BIN="prog_no_arg"
BUILD_DIR="./build"
CFLAGS="-Wall -m32 -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIB="../lib/"
OBJS="../build/interrupt.o ../build/timer.o 	../build/init.o ../build/main.o \ 
	../build/kernel.o ../build/print.o ../build/debug.o ../build/string.o	\
	../build/bitmap.o ../build/memory.o ../build/thread.o ../build/list.o	\
	../build/switch.o ../build/sync.o ../build/console.o ../build/keyboard.o \
	../build/ioqueue.o ../build/tss.o ../build/process.o ../build/syscall.o	\
	../build/syscall-init.o ../build/stdio.o ../build/stdio-kernel.o		\
	../build/ide.o ../build/fs.o ../build/file.o ../build/inode.o		\
	../build/dir.o ../build/fork.o ../build/shell.o ../build/buildin_cmd.o	\
	../build/exec.o ../build/assert.o"
DD_IN=$BIN
DD_OUT="/root/loong-OS/bin/hd60M.img"

gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN

if [ -f $BIN ];then
	dd if=./$DD_IN of=$DD_OUT bs=512 \
	count=30 seek=300 conv=notrunc
fi
