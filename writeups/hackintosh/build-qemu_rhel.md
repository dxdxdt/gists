Since we're going to install stuff in `/usr/local`, the subsystems should be set
up accordingly. Check if it's already set up on the system. You probably have to
do both of these if the system is fresh from install.

```sh
# Check ldconfig
sudo ldconfig -v | grep /usr/local
# should yield:
# /usr/local/lib: (from /etc/ld.so.conf.d/...)
# /usr/local/lib64: (from /etc/ld.so.conf.d/...)

# Check pkgconf
echo $PKG_CONFIG_PATH
# should yield:
# .../usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig...
```

If the ldconfig setting is missing:

```sh
sudo su -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/usr-local.conf'
sudo su -c 'echo "/usr/local/lib64" >> /etc/ld.so.conf.d/usr-local.conf'
```

If the pkgconf setting is missing, create the file at
`/etc/profile.d/usr-local.sh`:

```
if [ -z "$PKG_CONFIG_PATH" ]; then
	export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
else
	export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig"
fi
```

Install the dependencies and tools needed to build qemu.

```sh
sudo dnf install\
	git\
	curl\
	make\
	tar\
	bison\
	flex\
	meson\
	gcc-c++\
\
	zlib-devel\
\
	openssl-devel\
\
	ncurses-devel\
	bzip2-devel\
	glib2-devel\
\
	usbredir-devel\
	libusb-devel\
	pixman-devel\
	libgudev-devel\
	spice-protocol\
	opus-devel\
	libjpeg-turbo-devel\
	libepoxy-devel\
\
	edk2-ovmf
```

For when things go wrong (you don't need these unless you're an expert)

```sh
sudo dnf install gdb setroubleshoot-server
```

Add fcontext entries for the custom built qemu. This doc may be outdated and the
fcontext entries for qemu in your distro may have been changed. Check the output
of the following command to see if there's any discrepancy. Basically, what has
to be done is making `/usr/local` variants for each qemu executable entry.

```sh
sudo semanage fcontext -l | egrep '/usr/.*/qemu'
```

For Rocky 9, the entries looked like this. Feel free to use it if you see no
difference.

```sh
sudo semanage fcontext -a -t qemu_exec_t                    '/usr/local/bin/qemu'
sudo semanage fcontext -a -t virt_qemu_ga_exec_t            '/usr/local/bin/qemu-ga'
sudo semanage fcontext -a -t qemu_exec_t                    '/usr/local/bin/qemu-kvm'
sudo semanage fcontext -a -t virtd_exec_t                   '/usr/local/bin/qemu-pr-helper'
sudo semanage fcontext -a -t virtd_exec_t                   '/usr/local/bin/qemu-storage-daemon'
sudo semanage fcontext -a -t qemu_exec_t                    '/usr/local/bin/qemu-system-.*'
sudo semanage fcontext -a -t virt_bridgehelper_exec_t       '/usr/local/libexec/qemu-bridge-helper'
sudo semanage fcontext -a -t virt_qemu_ga_exec_t            '/usr/local/libexec/qemu-ga(/.*)?'
sudo semanage fcontext -a -t virt_qemu_ga_unconfined_exec_t '/usr/local/libexec/qemu-ga/fsfreeze-hook.d(/.*)?'
sudo semanage fcontext -a -t virtd_exec_t                   '/usr/local/libexec/qemu-pr-helper'
sudo semanage fcontext -a -t qemu_exec_t                    '/usr/local/libexec/qemu.*'
```

Get the sources. dmg2img is not on RHEL repos either so it has to be built from
the source as well.

```sh
git clone https://github.com/Lekensteyn/dmg2img
curl -LO https://download.qemu.org/qemu-8.2.2.tar.xz
curl -LO https://www.spice-space.org/download/releases/spice-server/spice-0.15.2.tar.bz2
```

Build and install.

```sh
pushd dmg2img
make && sudo make install
popd

tar xf spice-0.15.2.tar.bz2
pushd spice-0.15.2
./configure
make -j$(nproc) && sudo make install
popd

tar xf qemu-8.2.2.tar.xz
pushd qemu-8.2.2
./configure \
	--target-list=x86_64-softmmu \
	--enable-debug \
	--enable-docs \
	--enable-vnc \
	--enable-spice-protocol \
	--enable-curses \
	--enable-libusb \
	--enable-usb-redir \
	--enable-libudev \
	--enable-slirp \
	--enable-spice \
	--enable-opengl
make -j$(nproc) && make -j$(nproc) test && sudo make install
popd

# qemu-kvm is just a symbolic link to the host arch qemu
# qemu makefile doesn't do it for us
sudo ln -s /bin/qemu-system-x86_64 /bin/qemu-kvm
```
