# libopendps

A library for controlling [OpenDPS](https://github.com/kanflo/opendps) via UART.

## How to build
```
mkdir build && cd build
cmake ..
make
```

## Command line utility

libopendps includes a command line utility dpsctl.

Control DPS on tty ttyUSB0 with baudrate 9600.
Ping DPS (DPS screen will blink once).
```
$dpsctl -d /dev/ttyUSB0 -b 9600 -p
```

Control DPS on tty ttyUSB0 with baudrate 9600.
Set voltage to 3300mV and current to 1000mA.
Enable output.
```
$dpsctl -d /dev/ttyUSB0 -b 9600 -V 3300 -c 1000 -o
```
