# QRET Launch Control Protocol

The QLCP protocol is a binary protocol used by launch control for ground communications between the launch control server and control nodes. It supports remote actuator control and data acquisition from control nodes for pre-launch systems control.

---

## Network Configuration

### SSDP Discovery

The server announces its presence via SSDP multicast. Devices listen for this broadcast to discover the server.

- Multicast address: `239.255.255.250`
- Multicast port: `1900`
- Search target: `urn:qretprop:espdevice:1`

The server sends the following M-SEARCH packet:

```
M-SEARCH * HTTP/1.1\r\n
HOST: 239.255.255.250:1900\r\n
MAN: "ssdp:discover"\r\n
MX: 2\r\n
ST: urn:qretprop:espdevice:1\r\n
USER-AGENT: QRET/1.0\r\n
\r\n
```

When a device receives this packet, it extracts the server's IP address from the UDP source address of the M-SEARCH packet. It then opens a TCP connection to the server.

### TCP

- Server listen port: `50000`
- The server never connects to devices. Devices always initiate TCP connections to the server.

### UDP

- Server listen port: `50001`
- The server listens through UDP for DATA packets ONLY. DATA packets MUST be sent through UDP.

---

## More Information

Refer to PROTOCOL_SPECIFICATION.md for an in-depth explanation of all packet types and enums.

---

## Build Instructions

To compile the library as a CMake package, run:

```bash
cmake -S . -B build/ -G <build_system>
cmake --build build/
cmake --install build/ --prefix ./dist
```

If using as a Git submodule add:
```cmake
add_subdirectory(qlcp)
target_link_libraries(your_project_name PRIVATE qlcp::qlcp)
```
to your CMakeLists.txt.