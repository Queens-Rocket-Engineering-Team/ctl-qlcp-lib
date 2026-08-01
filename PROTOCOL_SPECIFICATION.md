# QRET Launch Control Protocol - Wire Format Specification

## 1. Introduction

This document is the authoritative wire-format reference for the QRET Launch Control binary protocol. All devices must implement this protocol to communicate with the Launch Control Server.

---

## 2. Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in RFC 2119 and RFC 8174.

---

## 3. Protocol Conventions

### 3.1 Size Units

All sizes specified in this document are in bytes.

### 3.2 Byte Order

All multi-byte values SHALL be represented in big-endian (network byte order).

### 3.3 Sequence Numbers

Each endpoint maintains an independent wrapping 8-bit sequence counter (0-255).

The sequence counter SHALL increment after every packet transmitted.

When a packet acts as a response to a previous request, the response SHALL include the sequence number of the original packet in its response correlation field.

Sequence numbers are used for request/response matching and packet tracking. They do not imply delivery guarantees.

### 3.4 Time Bases

QLCP uses three related time representations.

#### 3.4.1 Device Time

Device Time is the local monotonic clock maintained by the device.

- Units: microseconds
- Epoch: device boot
- Not synchronized between devices

#### 3.4.2 Server Time

Server Time is the monotonic clock maintained by the server.

- Units: microseconds
- Epoch: server startup
- Used as the canonical system reference time

#### 3.4.3 Protocol Timestamp

The TIMESTAMP field contained in packet headers.

Before the initial time synchronization, the protocol timestamp is equivalent to Device Time.

After a successful time synchronization, the protocol timestamp represents the device's estimate of Server Time.

All device-originated timestamps SHOULD be interpreted as Server Time after a successful time synchronization has completed.

### 3.5 ID Mapping

QLCP uses compact numeric IDs on the wire. These IDs are derived from the CONFIG packet.

Sensor IDs and control IDs use separate zero-based ID spaces.

A `control_id` is the zero-based ordinal of a control entry in the `controls` object, using the member order as serialized in the CONFIG JSON.

A `sensor_id` is the zero-based ordinal of a sensor entry found by traversing `sensor_info`, using the member order as serialized in the CONFIG JSON. Each category is traversed sequentially in the order defined by the CONFIG JSON.

Although JSON objects are formally unordered, QLCP assigns semantic meaning to member order when deriving `sensor_id` and `control_id` values.

Implementations MUST preserve object member order exactly as transmitted.

Sensor and control IDs are session-local. They are derived from the CONFIG packet received during device registration and MUST NOT be assumed stable across different CONFIG versions.

---

## 4. Transport

QLCP uses multiple network transport protocols depending on the traffic type.

### 4.1 Discovery Transport

DISCOVERY packets are transmitted using UDP multicast to:

`239.100.0.1:10000`

Devices that are not currently connected to the server MUST listen for DISCOVERY packets and use them to determine the server address.

### 4.2 Control Transport

Connection is established using TCP on server port `50000`.

The following packet types are transmitted over a persistent TCP connection:

- ESTOP
- CONFIG
- HEARTBEAT
- TIMESYNC_REQ
- TIMESYNC_RESP
- CONTROL
- STATUS_REQUEST
- STREAM_START
- STREAM_STOP
- GET_SINGLE
- STATUS
- ACK
- NACK

TCP is used because these messages require reliable delivery, ordering, and request/response semantics.

### 4.3 Telemetry Transport

DATA packets are sent as UDP datagrams to server port `50001`.

DATA packets are transported over UDP.

UDP is used to eliminate latency and avoid head-of-line blocking caused by TCP retransmission. Occasional packet loss is acceptable for real-time telemetry streams.

Each DATA packet is self-contained and identified by its timestamp.

---

## 5. Packet Header

Every packet begins with the same 17-byte header. This includes basic metadata about each packet and is followed by the applicable packet-specific payload.

```
Offset  Size  Type    Field        Description
------  ----  ------  -----------  ----------------------------------------
0       4     ASCII   MAGIC_NUM    Magic number to verify framing - Always 0x514C4350 ("QLCP")
4       1     uint8   VERSION      Protocol revision (currently 0x03)
5       1     uint8   PACKET_TYPE  Packet type enum (see below)
6       1     uint8   SEQUENCE     Wrapping counter 0-255 for req/resp matching
7       2     uint16  LENGTH       Total packet size including this header
9       8     uint64  TIMESTAMP    Protocol timestamp; see Section 3.4 Time Bases
```

The interpretation of TIMESTAMP is defined in Section 3.4 Time Bases and Section 7.7 TIMESYNC.

### 5.1 TCP Framing

To parse a TCP stream:

1. Read bytes until the 4-byte sequence 0x514C4350 is found.
2. Read 13 more bytes (remainder header; 17 byte total header).
3. Extract LENGTH from bytes 7-8.
4. Read `LENGTH - 17` more bytes (payload).
5. Decode the complete packet.

---

## 6. Packet Types

```
Value  Name            Direction        Description
-----  --------------  ---------------  --------------------------------
0x00   ESTOP           Server -> Device Emergency stop, highest priority
0x01   DISCOVERY       Server -> *      Discovery broadcast
0x02   HEARTBEAT       Server -> Device Keep-alive
0x03   CONTROL         Server -> Device Control command (valve, etc.)
0x04   STATUS_REQUEST  Server -> Device Request device status
0x05   STREAM_START    Server -> Device Start streaming at given Hz
0x06   STREAM_STOP     Server -> Device Stop streaming
0x07   GET_SINGLE      Server -> Device Request single data reading
0x08   TIMESYNC_RESP   Server -> Device Time synchronization response

0x09   TIMESYNC_REQ    Device -> Server Time synchronization request
0x10   CONFIG          Device -> Server Device configuration (JSON)
0x11   DATA            Device -> Server Sensor readings from a single acquisition interval
0x12   STATUS          Device -> Server Device status response

0x13   ACK             Any -> Any Positive acknowledgment
0x14   NACK            Any -> Any Negative acknowledgment with error

0xFF   NO_ACK          Never sent   Sentinel ack_packet_type for unsolicited STATUS packets. Never set as packet_type in a header.
```

---

## 7. Packet Definitions

### 7.1 Header-Only Packets

Length: 17 bytes

These packets have no payload.

| Packet Type    | Value |
|----------------|-------|
| ESTOP          | 0x00  |
| DISCOVERY      | 0x01  |
| HEARTBEAT      | 0x02  |
| STATUS_REQUEST | 0x04  |
| STREAM_STOP    | 0x06  |
| GET_SINGLE     | 0x07  |
| TIMESYNC_REQ   | 0x09  |

---

### 7.2 STATUS

Length: Variable, 20 + 7*N bytes, where N is the number of controls.

Direction: Device → Server

Purpose: Reports the current device control states. STATUS is sent either in
response to CONTROL and STATUS_REQUEST packets, or unsolicited as a
device-initiated status update.

The STATUS packet represents the device state after the request has been
processed, or the current device state at time of sending for an unsolicited
STATUS.

When sending an unsolicited STATUS update (that is, not a response to a CONTROL
or STATUS_REQUEST), the device MUST set `ack_packet_type` to `NO_ACK` (`0xFF`),
and MAY use any `ack_sequence`.

The server MUST ignore command response tracking on STATUS updates with
`ack_packet_type` set to `NO_ACK`, and otherwise MUST process the STATUS update
as normal.

For each control:

- If `control_status` is `CONFIRMED`, `control_state` represents the current state of that control
- If `control_status` is `PENDING`, `control_state` represents the last commanded state of that control
- If `control_status` is `ERROR`, `control_state` is undefined and the value should not be used

These statuses exist for situations where the device may need acknowledgement from an external node on control status.

The `count` field specifies the number of control states
contained in the packet.

Each `control_id` corresponds to the index of the relevant control as defined in Section 3.5 ID Mapping.

```
Offset  Size  Type    Field            Description
------  ----  ------  ------           -------------------------
0-16    17    -       header           Standard header
17      1     uint8   ack_packet_type  Type of packet being acknowledged
18      1     uint8   ack_sequence     Sequence number of packet being acknowledged
19      1     uint8   count            Number of valves/controls (N)

Repeated N times (7 bytes each):
+0      1     uint8    control_id       Index in device's control array
+1      1     uint8    control_type     Control's type from control type enum
+2      1     uint8    control_status   Control's confirmation status from control status enum
+3      4     variable control_state    Control's state, interpreted per control_status
```

---

### 7.3 STREAM_START

Length: 19 bytes

Direction: Server → Device

Purpose: Command the device to start streaming DATA packets at a specified frequency. Receiving STREAM_START while already streaming SHALL update the active stream frequency to the newly requested value.

If the request frequency exceeds the device's capabilities, the device SHOULD stream at the highest supported frequency that does not exceed the requested frequency.

The `frequency_hz` field specifies the requested DATA transmission frequency in Hertz.

```
Offset  Size  Type    Field         Description
------  ----  ------  ------------  -------------------------
0-16    17    -       header        Standard header
17      2     uint16  frequency_hz  DATA transmission frequency (1-65535)
```

---

### 7.4 CONTROL

Length: 23 bytes

Direction: Server → Device

Purpose: Command the device to set the state of a valve/control. Upon receipt, the device MUST set the state of the specified valve/control to the value specified in the `control_state` field.

The `control_id` field specifies the index of the valve/control as specified in Section 3.5 ID Mapping.

```
Offset  Size  Type    Field           Description
------  ----  ------  -------------   -------------------------
0-16    17    -       header          Standard header
17      1     uint8    control_id     Index in device's control array
18      1     uint8    control_type   Control's type from control type enum
19      4     variable control_state  Control's desired state
```

---

### 7.5 ACK

Length: 19 bytes

Direction: Any -> Any

Purpose: Indicate successful processing of a request.

An ACK packet confirms that the referenced packet was successfully processed.

The `ack_packet_type` and `ack_sequence` fields identify the packet being acknowledged.

```
Offset  Size  Type    Field           Description
------  ----  ------  --------------  -------------------------
0-16    17    -       header          Standard header (type=0x13)
17      1     uint8   ack_packet_type Type of packet being acknowledged
18      1     uint8   ack_sequence    Sequence number of acknowledged packet
```

---

### 7.6 NACK

Length: 20 bytes

Direction: Any -> Any

Purpose: Indicate unsuccessful processing of a request.

A NACK packet confirms that the referenced packet could not be successfully processed.

The `nack_packet_type` and `nack_sequence` fields identify the rejected packet.

The `error_code` field specifies the reason for rejection.

```
Offset  Size  Type    Field            Description
------  ----  ------  ---------------  -------------------------
0-16    17    -       header           Standard header (type=0x14)
17      1     uint8   nack_packet_type Type of packet being rejected
18      1     uint8   nack_sequence    Sequence number of rejected packet
19      1     uint8   error_code       ErrorCode enum value
```

---

### 7.7 TIMESYNC

Purpose: Synchronize the device clock to the server clock using a four-timestamp round-trip exchange. The protocol estimates one-way network latency and computes a clock offset that allows device timestamps to be expressed in the server's time base.

The server participates statelessly. It records timestamps and echoes values from the request but does not compute the clock offset.

#### 7.7.1 TIMESYNC_REQ

Length: 17 bytes

Direction: Device -> Server

Purpose: Initiate a clock synchronization exchange.

The device SHALL populate the packet header TIMESTAMP field with:
```
T1 = device_time_us()
```

The packet contains no payload.

#### 7.7.2 TIMESYNC_RESP

Length: 35 bytes

Direction: Server -> Device

Purpose: Complete a clock synchronization exchange.

Upon receiving a TIMESYNC_REQ, the server SHALL:
1. Record the receipt time:
```
T2 = server_time_us()
```

2. Immediately before transmitting the response, record:
```
T3 = server_time_us()
```

3. Populate the response payload with T1 and T2.

The response MUST reference the corresponding TIMESYNC_REQ using its response correlation fields.

```
Offset  Size  Type    Field           Description
------  ----  ------  ----------      -------------------------
0-16    17    -       header          Standard header (type=0x08, TIMESTAMP=T3)
17      1     uint8   ack_packet_type Type of packet being acknowledged
18      1     uint8   ack_sequence    Sequence of packet being acknowledged
19      8     uint64  t1_echo_us      Echo of T1 from request header
27      8     uint64  t2_us           Server receipt time (monotonic us, T2)
```

#### 7.7.3 Device behavior

On receiving TIMESYNC_RESP, the device SHALL:

1. Immediately sample:
```
T4 = device_time_us()
```
2. Recover the four timestamps:
```
T1 = resp.payload.t1_echo_us      (device send time, echoed from request header)
T2 = resp.payload.t2_us           (server receipt time)
T3 = resp.header.timestamp_us     (server send time)
T4 = device_time_us()             (sampled in step 1)
```
3. Compute the clock offset:
```
ts_offset = ((T1 - T2) + (T4 - T3)) / 2
```
4. Store the computed offset.

The computed offset represents:
```
ts_offset = device_time_us - server_time_us
```
A positive offset indicates that the device clock is ahead of the server clock.

For all subsequent outgoing packets, the device SHALL populate the header TIMESTAMP field with:
```
header.timestamp_us = device_time_us() - ts_offset
```

After synchronization, device-originated packet timestamps SHALL be expressed in the server's time base.

#### 7.7.4 Synchronization Schedule

The device SHALL initiate a synchronization cycle:
1. Immediately after receiving the ACK for its CONFIG packet.
2. At least once every 60 seconds during normal operation.

#### 7.7.5 Notes / Rationale

Synchronizing device timestamps to the server clock allows telemetry timing to be derived from the device oscillator rather than network receive times. This reduces timing jitter caused by TCP buffering, operating-system scheduling, and network latency.

ESP32 crystal oscillators drift approximately 10 ppm. Over 1 minute this is ~0.6 ms; over 1 hour it is ~36 ms. At high sample rates (hundreds of Hz), where one sample period is 1–10 ms, this drift becomes significant during long test runs. The 1-minute maintenance interval keeps accumulated drift under ~0.6 ms between corrections.

---

### 7.8 DATA

Length: 18 + 5*N bytes, where N is the number of sensors.

Direction: Device -> Server

Purpose: Report sensor readings from a single acquisition interval. Contains up to one reading for each sensor on the device.

The packet header TIMESTAMP field represents the acquisition timestamp of the readings contained in the packet.

Each `sensor_id` corresponds to the index of the relevant sensor as specified in Section 3.5 ID Mapping.

```
Offset  Size  Type    Field     Description
------  ----  ------  --------  -------------------------
0-16    17    -       header    Standard header
17      1     uint8   count     Number of sensor readings (N)

Repeated N times (5 bytes each):
+0      1     uint8   sensor_id  Index in device's sensor array
+1      4     float32 value      IEEE 754 single-precision float
```

---

### 7.9 CONFIG

Length: 17 + packet_len bytes

Direction: Device -> Server

Purpose: Device configuration sent on connection. Defines the device's control and sensor configuration. json_len is found from the packet_len field in the header.

```
Offset      Size      Type    Field       Description
------      ----      ------  ----------  -------------------------
0-16        17        -       header      Standard header
17          json_len  bytes   json_data   UTF-8 encoded JSON string
```

---

## 8. Packet Size Summary

| Packet         | Total Size       | Payload after header |
|----------------|------------------|----------------------|
| ESTOP          | 17               | (none)               |
| DISCOVERY      | 17               | (none)               |
| HEARTBEAT      | 17               | (none)               |
| STREAM_STOP    | 17               | (none)               |
| GET_SINGLE     | 17               | (none)               |
| TIMESYNC_REQ   | 17               | (none)               |
| TIMESYNC_RESP  | 35               | 1B type + 1B seq + 8B T1_echo + 8B T2   |
| STATUS_REQUEST | 17               | (none)               |
| STREAM_START   | 19               | 2B frequency_hz      |
| CONTROL        | 23               | 1B cmd_id + 1B cmd_type + 4B state |
| ACK            | 19               | 1B type + 1B seq |
| NACK           | 20               | 1B type + 1B seq + 1B error |
| STATUS         | 20 + 7*N         | 1B type + 1B seq + 1B count + N*(1B+1B+1B+4B) |
| DATA           | 18 + 5*N         | 1B count + N*(1B+4B) |
| CONFIG         | 17 + json_len    | json_data   |

---

## 9. Enum Values

### 9.1 ControlType

```
Value  Name
-----  ------
0x00   BOOL
0x01   UINT32
0x02   INT32
0x03   FLOAT32
```

### 9.2 ControlStatus

```
Value  Name
-----  -----
0x00   CONFIRMED
0x01   PENDING
0xFF   ERROR
```

### 9.3 ControlState (For bool type controls)

```
Value  Name
-----  ------
0x00   CLOSED
0x01   OPEN
```

### 9.4 ErrorCode

```
Value  Name
-----  --------------
0x00   NONE            No error
0x01   UNKNOWN_TYPE    Unrecognized packet type
0x02   INVALID_ID      Invalid sensor/control ID
0x03   HARDWARE_FAULT  Hardware error
0x04   BUSY            Device busy
0x05   NOT_STREAMING   Not currently streaming
0x06   INVALID_PARAM   Invalid parameter value
```

---

## 10. Device Behavior Requirements

This section defines what the device must do when it receives each server command.

### 10.1 Packets Requiring Responses

The device MUST send a response for the following packet types:

| Packet Type  | Required Response |
|--------------|-------------------|
| HEARTBEAT    | ACK |
| TIMESYNC_RESP| ACK |
| STREAM_START | ACK (or NACK on error) |
| STREAM_STOP  | ACK |
| STATUS_REQUEST | STATUS |
| CONTROL      | STATUS |
| GET_SINGLE   | DATA |

The server MUST send a response for the following packet types:

| Packet Type  | Required Response |
|--------------|-------------------|
| TIMESYNC_REQ | TIMESYNC_RESP |
| CONFIG       | ACK |

### 10.2 DISCOVERY

The server broadcasts DISCOVERY packets to the multicast group 239.100.0.1:10000. When the device is disconnected from the server, it MUST listen to this multicast group and search for the DISCOVERY packet to get the server's IP.

### 10.3 ESTOP

On receiving ESTOP, the device MUST immediately set **all controls to their default states** as defined in the device's configuration (the `default_state` field of each control). This is the safest state for the hardware.

The device must respond with a STATUS packet containing its control states after the ESTOP.

### 10.4 GET_SINGLE

On receiving GET_SINGLE, the device MUST take **one reading from every sensor** and send single DATA packet containing one reading from every sensor.

### 10.5 CONTROL

On receiving CONTROL, the device MUST set the control state and send a STATUS packet with its current control states. The device MUST ensure that the control type sent in CONTROL matches the expected control type for the control id, as defined in its config.

### 10.6 STATUS_REQUEST

On receiving STATUS_REQUEST, the device MUST send a STATUS packet with its current control states.

### 10.7 Unknown Packet Types

If the device receives a packet with an unrecognized PACKET_TYPE, it MUST respond with a NACK using error code `UNKNOWN_TYPE` (0x01).

---

## 11. Connection Flow

```
 Server                                Device
   |                                     |
   |------------ DISCOVERY ----------->> |  1. Server multicasts on 239.100.0.1:10000
   |                                     |
   |<<----------- TCP connect -----------|  2. Device opens TCP to server:50000
   |                                     |
   |<<----------- CONFIG packet ---------|  3. Device sends JSON configuration
   |                                     |
   |------------ ACK ---------------->>  |  4. Server acknowledges config
   |                                     |
   |<<------- TIMESYNC_REQ packet -------|  5. Device sends timesync request
   |                                     |
   |--------- TIMESYNC_RESP --------->>  |  6. Server sends time reference
   |                                     |
   |<<------------ ACK ------------------|  7. Device acknowledges (server records sync)
   |                                     |
   |        (normal operation)           |
   |                                     |
   |------------ HEARTBEAT ---------->>  |  Periodic keep-alive (every 5s)
   |<<------------ ACK ------------------|
   |                                     |
   |<<------- TIMESYNC_REQ packet -------|  Periodic resync (every 1 min)
   |--------- TIMESYNC_RESP --------->>  |
   |<<------------ ACK ------------------|
   |                                     |
   |------------ STREAM_START ------->>  |  Start data streaming
   |<<------------ ACK ------------------|
   |<<------------ DATA -----------------|  Continuous sensor data at requested Hz
   |<<------------ DATA -----------------|
   |                                     |
   |------------ CONTROL ------------>>  |  Valve/actuator command
   |<<----------- STATUS ----------------|
   |                                     |
   |------------ STREAM_STOP -------->>  |  Stop streaming
   |<<------------ ACK ------------------|
   |                                     |
   |------------- ESTOP ------------->>  |  Emergency stop
   |<<----------- STATUS ----------------|  Device sets all controls to defaults
   |                                     |
```

Key points:
- The first packet on a new TCP connection is always CONFIG from the device.
- Sequence numbers in ACK/NACK match the sequence of the original request.
- Device sends TIMESYNC_REQ immediately after CONFIG ACK, then every 1 minute.
- DATA packet timestamps come from the device clock, using the previous timesync to calculate offset

---

## 12. CONFIG JSON Structure

The CONFIG packet carries a JSON object describing the device's capabilities. The server uses this to register sensors and controls.

### 12.1 Schema

Sensor and control objects must include a unit field. All other fields are not read by the server, and are only used for device setup. Therefore, they are flexible. Sensor and control groups are parsed from the server to group them.

```json
{
    "device_name": "<string>",

    "sensors": {
        "thermocouple": {
            "<name>": {
                "sensor_index": "<string>",
                "type": "<string>",
                "unit": "<string>"
            }
        },
        "pressure_transducer": {
            "<name>": {
                "sensor_index": "<string>",
                "resistor_ohms": "<float>",
                "max_pressure_PSI": "<int>",
                "unit": "<string>"
            }
        }
    },
    "controls": {
        "valve": {
            "<name>": {
                "control_index": "<string>",
                "type": "<string>",
                "default_state": "<string>",
                "unit": "<string>"
            }
        },
        "heater": {
            "<name>": {
                "control_index": "<string>",
                "type": "<string>",
                "default_state": "<float>",
                "unit": "<string>"
            }
        }
    }
}
```

### 12.2 Example

[Link to firmware repo config example.](https://github.com/Queens-Rocket-Engineering-Team/ctl-node-firmware/blob/main/esp_config.json)
