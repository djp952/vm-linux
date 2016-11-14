
busybox 1.25.1
--------------

>> Build: x86 on x86 host or x86_64 on x86_64 host

wget http://busybox.net/downloads/busybox-1.25.1.tar.bz2
tar xjvf busybox-1.25.1.tar.bz2
pushd busybox-1.25.1
make defconfig && make install CONFIG_STATIC=y
popd

>> Build x86 on x86_64 host

wget http://busybox.net/downloads/busybox-1.25.1.tar.bz2
tar xjvf busybox-1.25.1.tar.bz2
pushd busybox-1.25.1
make CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 defconfig && make CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32 install CONFIG_STATIC=y
popd