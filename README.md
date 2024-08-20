[![Build Status](https://gitlab.com/Northern.tech/Mender/mender-mcu-integration/badges/next/pipeline.svg)](https://gitlab.com/Northern.tech/Mender/mender-mcu-integration/pipelines)

# mender-mcu-integration

Mender is an open source over-the-air (OTA) software updater for embedded devices. Mender
comprises a client running at the embedded device, as well as a server that manages deployments
across many devices.

Mender provides modules to integrate with Real Time Operating Systems (RTOS). The code can be found
in [`mender-mcu` repository](https://github.com/mendersoftware/mender-mcu/).

This repository contains a reference project on how to integrate a user application with Mender OTA
Zephyr Module, using QEMU as the board of choice.

-------------------------------------------------------------------------------

![Mender logo](https://github.com/mendersoftware/mender/raw/master/mender_logo.png)


## Project Status

This repository is a work in progress. As we continue development, features and functionality may
evolve significantly.


## Getting started

Since the project is under active development, we recommend watching the repository or checking back
regularly for updates. Detailed documentation and usage instructions will be provided as the project
progresses.

To start using Mender, we recommend that you begin with the Getting started
section in [the Mender documentation](https://docs.mender.io/).


## Building the Zephyr Project Mender reference app

To add Mender OTA Zephyr Module on your project, create first a vanilla Zephyr workspace and
manually set the manifest to point to this repository.

```
west init workspace --manifest-url https://github.com/mendersoftware/mender-mcu-integration
cd workspace && west update
```

Now build and run the reference application with:

:warning: Work in progress :warning:

```
west build --build-dir build-qemu --board qemu_x86 mender-mcu-integration
./tools/net-tools/loop-socat.sh &
west build --build-dir build-qemu --target run
```

Inspiration:
* Look at overlays at https://github.com/zephyrproject-rtos/zephyr/tree/main/tests/subsys/dfu
* Maybe the tests at https://github.com/zephyrproject-rtos/zephyr/tree/main/tests/boot
* And learn about Sysbuild (?) https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/sysbuild/with_mcuboot

This looks promising
```
$ git diff
diff --git a/boards/qemu/x86/qemu_x86.dts b/boards/qemu/x86/qemu_x86.dts
index 907e8a1ec1c..86ddcf5bab9 100644
--- a/boards/qemu/x86/qemu_x86.dts
+++ b/boards/qemu/x86/qemu_x86.dts
@@ -147,7 +147,8 @@
                 * Storage partition will be used by FCB/LittleFS/NVS
                 * if enabled.
                 */
-               storage_partition: partition@1000 {
+               /*storage_partition: partition@1000 {*/
+               boot_partition: partition@1000 {
                        label = "storage";
                        reg = <0x00001000 0x00010000>;
                };
```

### Build for Nordic Semi eval board

https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html#flashing

https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/samples/subsys/usb/dfu/README.html

#### Flash the bootloader first

https://docs.mcuboot.com/readme-zephyr

```
west build --build-dir build-nordic-boot --board nrf52840dk/nrf52840 bootloader/mcuboot/boot/zephyr
west flash --build-dir build-nordic-boot
```

#### Now the application

Open a the serial:
```
minicom -D /dev/ttyACM1
```

And build, flash, blink:
```
west build --build-dir build-nordic-app --board nrf52840dk/nrf52840 mender-mcu-integration -- '-DCONFIG_MCUBOOT_SIGNATURE_KEY_FILE="bootloader/mcuboot/root-rsa-2048.pem"'
west flash --build-dir build-nordic-app
```

#### To reset all

```
nrfjprog --eraseall && nrfjprog --recover
```

## Contributing

We welcome and ask for your contribution. If you would like to contribute to
Mender, please read our guide on how to best get started
[contributing code or documentation](https://github.com/mendersoftware/mender/blob/master/CONTRIBUTING.md).


## License

Mender is licensed under the Apache License, Version 2.0. See
[LICENSE](https://github.com/mendersoftware/mender-mcu-integration/blob/master/LICENSE)
for the full license text.


## Security disclosure

We take security very seriously. If you come across any issue regarding
security, please disclose the information by sending an email to
[security@mender.io](security@mender.io). Please do not create a new public
issue. We thank you in advance for your cooperation.


## Connect with us

* Join the [Mender Hub discussion forum](https://hub.mender.io)
* Follow us on [Twitter](https://twitter.com/mender_io). Please
  feel free to tweet us questions.
* Fork us on [Github](https://github.com/mendersoftware)
* Create an issue in the [bugtracker](https://northerntech.atlassian.net/projects/MEN)
* Email us at [contact@mender.io](mailto:contact@mender.io)
* Connect to the [#mender IRC channel on Libera](https://web.libera.chat/?#mender)


## Authors

Mender was created by the team at [Northern.tech AS](https://northern.tech),
with many contributions from the community. Thanks
[everyone](https://github.com/mendersoftware/mender/graphs/contributors)!

[Mender](https://mender.io) is sponsored by [Northern.tech AS](https://northern.tech).


## Log to file for the win \o/

/home/lluis/.local/opt/zephyr-sdk-0.16.5-1/sysroots/x86_64-pokysdk-linux/usr/bin/qemu-system-i386 -m 32 -cpu qemu32,+nx,+pae -machine q35 -device isa-debug-exit,iobase=0xf4,iosize=0x04 -nographic -machine acpi=off -net none -pidfile qemu.pid -chardev pty,id=con,mux=on,logfile=pty.log -serial chardev:con -mon chardev=con,mode=readline -icount shift=5,align=off,sleep=off -rtc clock=vm -kernel /home/lluis/west/build/qemu_x86/bootloader/mcuboot/boot/zephyr/zephyr/zephyr.elf

#### 64 bits

/home/lluis/.local/opt/zephyr-sdk-0.16.5-1/sysroots/x86_64-pokysdk-linux/usr/bin/qemu-system-x86_64 -m 32 -cpu qemu64,+x2apic,mmx,mmxext,sse,sse2 -machine q35 -device isa-debug-exit,iobase=0xf4,iosize=0x04 -nographic -machine acpi=off -net none -pidfile qemu.pid -chardev pty,id=con,mux=on,logfile=pty.log -serial chardev:con -mon chardev=con,mode=readline -device loader,file=/home/lluis/west/build/qemu_x86_64/bootloader/mcuboot/boot/zephyr/zephyr/zephyr-qemu-main.elf -smp cpus=2 -kernel /home/lluis/west/build/qemu_x86_64/bootloader/mcuboot/boot/zephyr/zephyr/zephyr-qemu-locore.elf

https://docs.mcuboot.com/serial_recovery.html

https://docs.zephyrproject.org/latest/hardware/emulator/index.html
https://docs.zephyrproject.org/latest/build/dts/api/bindings/flash_controller/zephyr%2Csim-flash.html#std-dtcompatible-zephyr-sim-flash
