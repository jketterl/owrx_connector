# OpenWebRX connectors

This project contains a set of connectors that are used by [OpenWebRX](https://github.com/jketterl/openwebrx) to
interface with SDR hardware.

For this purpose, there's two sockets on every connector:
* a data socket that relays the IQ stream read from the SDR device to multiple clients
* a control socket that allows the controlling instance to change SDR parameters at runtime (e.g. frequency, gain, ...)

By default, the IQ sample format is 32bit floats.

## rtl_tcp compatibility

The data socket will detect if the connected application is sending any data. If it does, it will assume t switch the output
format on the corresponding connection to 8bit unsigned int, which is accepted by many applications that support
rtl_tcp.

The connector does not evaluate any data on client connections, so your application will not be able to control the
SDR hardware.

## Installation

This project comes with a cmake build. It is recommended to build in a separate directory.

```
mkdir build
cd build
cmake ..
make
sudo make install
```
