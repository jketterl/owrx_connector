# OpenWebRX connectors

This project contains a set of connectors that are used by [OpenWebRX](https://github.com/jketterl/openwebrx) to
interface with SDR hardware.

For this purpose, there's two sockets on every connector:
* a data socket that relays the IQ stream read from the SDR device to multiple clients
* a control socket that allows the controlling instance to change SDR parameters at runtime (e.g. frequency, gain, ...)

By default, the IQ sample format is 32bit floats.

## rtl_tcp compatibility

The data socket will detect if the connected application is sending any data. If it does, it will assume that the
connected client is an application attempting to talk to rtl_tcp, and switch the output format on the corresponding
connection to 8bit unsigned int, which is the default format of rtl_tcp.

The connector does not evaluate any data on client connections, so rtl_tcp commands will be discarded. This means
that applications using rtl_tcp compatibiltiy will not be able to control the SDR hardware.

## CPU-Optimized builds

This project can profit, to some extent, from loop vectorization, if your CPU supports it. The compiler optimization
flags are disabled by default, but you can enable them by passing a custom build type argument to cmake:

```
cmake -DCMAKE_BUILD_TYPE=Optimized ..
```

## Dependencies

- If you want to work with rtlsdr devices, you will need to install the corresponding header files (on Debian:
  `apt-get install librtlsdr-dev`)
- If you want to work with devices supported by SoapySDR, you will need to install the corresponnding header files
  (on Debian: `apt-get install libsoapysdr-dev`)

If you have compiled rtlsdr / soapy from source, you should not need to worry about this. The installation should place
the headers in their correct locations.

## Installation

This project comes with a cmake build. It is recommended to build in a separate directory.

```
mkdir build
cd build
cmake ..
make
sudo make install
```
