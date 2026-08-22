export LOONG_ROOT="$(pwd -P)"

if [ ! -f "$LOONG_ROOT/makefile" ] ||
     [ ! -f "$LOONG_ROOT/bochsrc" ]; then
      echo "请在 loong-OS 仓库根目录执行 source ./tool/env.sh"
      return 1
  fi

mkdir -p "$LOONG_ROOT/.toolchains"

export TOOLCHAIN_ROOT="$LOONG_ROOT/.toolchains"
export GCC34_ROOT="$TOOLCHAIN_ROOT/opt/gcc-3.4"
export NASM_PREFIX="$TOOLCHAIN_ROOT/opt/nasm-2.15.05"
export BOCHS_PREFIX="$TOOLCHAIN_ROOT/opt/bochs-2.6.2"

export PATH="$TOOLCHAIN_ROOT/bin:$PATH"
export BXSHARE="$BOCHS_PREFIX/share/bochs"

mkdir -p \
      "$TOOLCHAIN_ROOT/src" \
      "$TOOLCHAIN_ROOT/opt" \
      "$TOOLCHAIN_ROOT/bin"

tar -xzvf "$LOONG_ROOT/tool/nasm-2.15.05.tar.gz" \
      -C "$TOOLCHAIN_ROOT/src"

cd "$TOOLCHAIN_ROOT/src/nasm-2.15.05"
  ./configure --prefix="$NASM_PREFIX"
  make -j"$(nproc)"
  make install

tar -xzvf "$LOONG_ROOT/tool/bochs-2.6.2.tar.gz" \
      -C "$TOOLCHAIN_ROOT/src"

cd "$TOOLCHAIN_ROOT/src/bochs-2.6.2"
./configure \
  --prefix="$BOCHS_PREFIX" \
  --enable-debugger \
  --enable-disasm \
  --enable-iodebug \
  --enable-x86debugger \
  --with-x \
  --with-x11 \
  --disable-plugins

make -j"$(nproc)"
make install

mkdir -p "$GCC34_ROOT"

for deb in \
  gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb \
  cpp-3.4_3.4.6-6ubuntu3_amd64.deb \
  gcc-3.4_3.4.6-6ubuntu3_amd64.deb
do
  ar p "$LOONG_ROOT/gcc3.4lib/$deb" data.tar.gz |
      tar -xzf - -C "$GCC34_ROOT"
done


cd "$TOOLCHAIN_ROOT/bin"

ln -sfn ../opt/gcc-3.4/usr/bin/gcc-3.4 gcc
ln -sfn ../opt/gcc-3.4/usr/bin/cpp-3.4 cpp
ln -sfn ../opt/nasm-2.15.05/bin/nasm nasm
ln -sfn ../opt/bochs-2.6.2/bin/bochs bochs
ln -sfn ../opt/bochs-2.6.2/bin/bximage bximage
ln -sfn ../opt/bochs-2.6.2/bin/bxcommit bxcommit

