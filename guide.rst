Upgrade DPS5005 to OpenDPS
--------------------------

Links
~~~~~

- https://johan.kanflo.com/upgrading-your-dps5005/
- https://github.com/kanflo/opendps

Setup
~~~~~

Clone opendps repository::

    git clone --recursive https://github.com/kanflo/opendps.git

Disable wifi in ``opendps/Makefile``::

    WIFI := 0

Cross compile::

    make -C libopencm3
    make -C opendps
    make -C dpsboot

Flash
~~~~~

Check that OpenOCD can connect to device::

    cd openocd/scripts
    openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg

Power on DPS and set output to disabled, then::

    ./ocd-client.py all > 5V-off.txt

Restart OpenOCD, toggle power of DPS and enable 5v output, then::

    ./ocd-client.py all > 5V-on.txt

Wipe flash::

    telnet localhost 4444
    reset halt
    flash erase_address unlock 0x08000000 0x10000

Expect an error. Restart OpenOCD, toggle power of DPS and repeat above step.

Then flash app and bootloader::

    make -C opendps flash
    make -C dpsboot flash

Restart DPS and firmware should now be updated.
