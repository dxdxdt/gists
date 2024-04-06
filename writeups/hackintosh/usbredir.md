That 10+ ms delay is apparently detrimental. It makes sense because USB is
designed to work over short distances. The protocol and (even) electrical
characteristics are built upon this assumption. The USB frames are short in
length and polled frequently, and almost no buffering is done in the background
as it works closely on the hardware. Therefore, the performance hit is
inevitable and instability issues are expected when USB frames are passed over
IP.

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

### TODO Analysis: Android and IOS Devices
This is work in progress.

Points:

- To make XNU kernel happy, the devices must be attached to a hub. The kernel
  cannot enumerate usb devices directly hot-plugged in the controller
  - Performance hit due to the fact that qemu only supports emulation of
    full-speed usb hub
- Massive round-trip delay over the network in terms of USB(which speaks in
  nanosecond terms)
  - As a result, it's possible that Android and IOS devices initiate USB reset
    rather than patiently sticking up with the delay (possibly)due to the
    limitation of hardware/firmware(TODO: inspect Logcat and Xcode instruments
    to verify it)
- TODO: Instead of using USB redirection, USB devices can be attached to the
  controller on guest start up
- There are many components that make the USB redirection over network possible.
  Quite possibly, the culprit could be one of the components along the chain
- The issue is present on other proprietary solutions using VHCI as
  well(FlexiHub, VirtualHere)
  - meaning that it's likely the "massive round-trip delay" issue
  - or it's a bug in the encapsulation protocol/implementation that's common in
    all proprietary/FOSS implementation

"The chain":

```
                  <- NET ->
CLIENT                                  HOST
tcp/ip                                  tcp/ip
ssh                                     ssh
spice+usbredir                          libvirt
libusb                                  spice
virt-viewer                             qemu
                                        xhci
                                        usb-hub
                                        usb-redir
```

"The components"

- virt
  - ssh tunneling
  - USB/IP
  - SPICE
  - libusb
  - usbredir
  - usb/ip
- QEMU
  - xhci
  - usb-hub
  - usb-redir
- Apple
  - Apple VHCI
