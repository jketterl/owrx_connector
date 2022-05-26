# OpenWebRX connectors

This project contains a set of connectors that are used by [OpenWebRX](https://github.com/jketterl/openwebrx) to
interface with SDR hardware.

For this purpose, there's two sockets on every connector:
* a data socket that relays the IQ stream read from the SDR device to multiple clients
* a control socket that allows the controlling instance to change SDR parameters at runtime (e.g. frequency, gain, ...)

By default, the IQ sample format is 32bit floats.

## `rtl_tcp` compatibility

For backwards compatibility, the connectors can additionally provide a secondary socket that provides data in a format
compatible with `rtl_tcp`. Use the `--rtltcp` option together with the port number you would like to use.

If necessary, the connector will resample the IQ data to an 8 bit data stream as required by the protocol. This means
that if your SDR device has a higher sample depth, a loss of data will occur, so other options should probably be
preferred.

The connectors do not evaluate any data on an `rtl_tcp` client connection, so any incoming commands will be discarded.
This means that applications using `rtl_tcp` compatibiltiy will not be able to control the SDR hardware.

## Dependencies

- Please install [csdr](https://github.com/jketterl/csdr) (version 0.18 or newer) before compiling this project.
- (optional) If you want to work with rtlsdr devices, you will need to install the corresponding header files (on 
  Debian: `apt-get install librtlsdr-dev`)
- (optional) If you want to work with devices supported by SoapySDR, you will need to install the corresponding header
  files (on Debian: `apt-get install libsoapysdr-dev`)

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
