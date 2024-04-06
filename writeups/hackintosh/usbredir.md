That 10 ms delay is apparently detrimental. It makes sense because USB is meant
to be work over short distances. The protocol and even the electrical
characteristics are built upon this assumption. USB frames are short in length
and polled frequently, and almost no buffering is done in the background as it
works closely on the hardware. Therefore, performance hit is inevitable and
instability issues are expected when USB frames are passed over IP.

The usb-redir devices are attached via qemu command line passthrough.
virt-manager is not "aware" of this and it doesn't even try to see if there usb
redirection is available.

If you really have to use it , use
[virt-viewer](https://virt-manager.org/download). It is available on most
distros and even on Windows.

### List of Devices
Some usb devices don't like getting passed around over the network.

| DEVICE | LOCAL | OVER IP |
|-|-|-|
| Mobile (Android, Apple) | 🫳kinda(unstable) | ❌NO |
| Flash Drives | ✅YES | 🫳kinda(slow) |
| HID(keyboard and mouse) | ✅YES | ✅YES |

### Analysis: Android and IOS Devices
TODO
