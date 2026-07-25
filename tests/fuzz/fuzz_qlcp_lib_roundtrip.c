#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "qlcp_lib.h"

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

static uint8_t g_encode_buffer[4096];
static qlcp_sensor_data g_encode_sensor_buffer[200];
static qlcp_status_data g_encode_control_buffer[200];
static uint8_t g_encode_config_buffer[2048];

static qlcp_sensor_data g_decode_sensor_buffer[200];
static qlcp_status_data g_decode_control_buffer[200];
static uint8_t g_decode_config_buffer[2048];

static int fuzz_s2c_packet(const uint8_t *data, size_t size);
static int fuzz_c2s_packet(const uint8_t *data, size_t size);
static bool check_s2c_equality(const qlcp_client_payload *p1, const qlcp_client_payload *p2);
static bool check_c2s_equality(const qlcp_server_payload *p1, const qlcp_server_payload *p2);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    memset(g_encode_buffer, 0, sizeof(g_encode_buffer));
    memset(g_encode_sensor_buffer, 0, sizeof(g_encode_sensor_buffer));
    memset(g_encode_control_buffer, 0, sizeof(g_encode_control_buffer));
    memset(g_encode_config_buffer, 0, sizeof(g_encode_config_buffer));

    memset(g_decode_sensor_buffer, 0, sizeof(g_decode_sensor_buffer));
    memset(g_decode_control_buffer, 0, sizeof(g_decode_control_buffer));
    memset(g_decode_config_buffer, 0, sizeof(g_decode_config_buffer));

    if (size < 1) {
        return 0;
    }
    // use first bit to decide whether to test c2s or s2c encoding/decoding
    if (data[0] & 0x01) {
        return fuzz_s2c_packet(data + 1, size - 1);
    } else {
        return fuzz_c2s_packet(data + 1, size - 1);
    }
}

int fuzz_s2c_packet(const uint8_t *data, size_t size) {

    if (size < 1) {
        return 0;
    }

    qlcp_client_payload encoded_payload = {0};

    // encode random packet
    switch (data[0]) {
    // header only packets (only need one case)
    case QLCP_PT_ESTOP:
        {
            qlcp_header_only_packet header_only_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(header_only_packet)) {
                return 0;
            }
            memcpy(&header_only_packet, data + 1, sizeof(header_only_packet));
            header_only_packet.packet_type = QLCP_PT_ESTOP;
            encoded_payload.packet_type = QLCP_PT_ESTOP;
            encoded_payload.payload_data.header_only = header_only_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_header_only(g_encode_buffer, &buffer_len, &header_only_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_ACK:
        {
            qlcp_ack_packet ack_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(ack_packet)) {
                return 0;
            }
            memcpy(&ack_packet, data + 1, sizeof(ack_packet));
            encoded_payload.packet_type = QLCP_PT_ACK;
            encoded_payload.payload_data.ack = ack_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_ack(g_encode_buffer, &buffer_len, &ack_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_NACK:
        {
            qlcp_nack_packet nack_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(nack_packet)) {
                return 0;
            }
            memcpy(&nack_packet, data + 1, sizeof(nack_packet));
            encoded_payload.packet_type = QLCP_PT_NACK;
            encoded_payload.payload_data.nack = nack_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_nack(g_encode_buffer, &buffer_len, &nack_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_CONTROL:
        {
            qlcp_control_packet control_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(control_packet)) {
                return 0;
            }
            memcpy(&control_packet, data + 1, sizeof(control_packet));
            encoded_payload.packet_type = QLCP_PT_CONTROL;
            encoded_payload.payload_data.control = control_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_control(g_encode_buffer, &buffer_len, &control_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_STREAM_START:
        {
            qlcp_stream_start_packet stream_start_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(stream_start_packet)) {
                return 0;
            }
            memcpy(&stream_start_packet, data + 1, sizeof(stream_start_packet));
            encoded_payload.packet_type = QLCP_PT_STREAM_START;
            encoded_payload.payload_data.stream_start = stream_start_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_stream_start(g_encode_buffer, &buffer_len, &stream_start_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_TIMESYNC_RESP:
        {
            qlcp_timesync_resp_packet timesync_resp_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(timesync_resp_packet)) {
                return 0;
            }
            memcpy(&timesync_resp_packet, data + 1, sizeof(timesync_resp_packet));
            encoded_payload.packet_type = QLCP_PT_TIMESYNC_RESP;
            encoded_payload.payload_data.timesync_resp = timesync_resp_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_timesync_resp(g_encode_buffer, &buffer_len, &timesync_resp_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    default:
        return 0;
    }

    qlcp_client_payload decoded_payload = {0};
    assert(
        qlcp_decode_server_to_client(&decoded_payload, g_encode_buffer, sizeof(g_encode_buffer)) == QLCP_OK
    );
    // check that the input payload is equal to the output from the decoder
    assert(
        check_s2c_equality(&encoded_payload, &decoded_payload)
    );

    return 0;
}

int fuzz_c2s_packet(const uint8_t *data, size_t size) {

    if (size < 1) {
        return 0;
    }

    qlcp_server_payload encoded_payload = {0};

    // encode random packet
    switch (data[0]) {
    // header only packets (only need one case)
    case QLCP_PT_ESTOP:
        {
            qlcp_header_only_packet header_only_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(header_only_packet)) {
                return 0;
            }
            memcpy(&header_only_packet, data + 1, sizeof(header_only_packet));
            header_only_packet.packet_type = QLCP_PT_TIMESYNC_REQ;
            encoded_payload.packet_type = QLCP_PT_TIMESYNC_REQ;
            encoded_payload.payload_data.header_only = header_only_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_header_only(g_encode_buffer, &buffer_len, &header_only_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_ACK:
        {
            qlcp_ack_packet ack_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(ack_packet)) {
                return 0;
            }
            memcpy(&ack_packet, data + 1, sizeof(ack_packet));
            encoded_payload.packet_type = QLCP_PT_ACK;
            encoded_payload.payload_data.ack = ack_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_ack(g_encode_buffer, &buffer_len, &ack_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_NACK:
        {
            qlcp_nack_packet nack_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(nack_packet)) {
                return 0;
            }
            memcpy(&nack_packet, data + 1, sizeof(nack_packet));
            encoded_payload.packet_type = QLCP_PT_NACK;
            encoded_payload.payload_data.nack = nack_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_nack(g_encode_buffer, &buffer_len, &nack_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_DATA:
        {
            qlcp_data_packet data_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(data_packet)) {
                return 0;
            }
            memcpy(&data_packet, data + 1, sizeof(data_packet));

            // copy into the sensor buffer
            const size_t struct_size = sizeof(g_encode_sensor_buffer[0]);
            const size_t max_structs = sizeof(g_encode_sensor_buffer) / struct_size;
            const size_t available_bytes = size - 1 - sizeof(data_packet);

            const size_t structs_to_copy = MIN(available_bytes / struct_size, max_structs);
            const size_t data_copy_len = structs_to_copy * struct_size;

            memcpy(&g_encode_sensor_buffer, data + 1 + sizeof(data_packet), data_copy_len);
            data_packet.sensor_data = g_encode_sensor_buffer;
            data_packet.sensor_count = structs_to_copy;

            encoded_payload.packet_type = QLCP_PT_DATA;
            encoded_payload.payload_data.data = data_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_data(g_encode_buffer, &buffer_len, &data_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_STATUS:
        {
            qlcp_status_packet status_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(status_packet)) {
                return 0;
            }
            memcpy(&status_packet, data + 1, sizeof(status_packet));

            // copy into the control buffer
            const size_t struct_size = sizeof(g_encode_control_buffer[0]);
            const size_t max_structs = sizeof(g_encode_control_buffer) / struct_size;
            const size_t available_bytes = size - 1 - sizeof(status_packet);

            const size_t structs_to_copy = MIN(available_bytes / struct_size, max_structs);
            const size_t data_copy_len = structs_to_copy * struct_size;
            
            memcpy(&g_encode_control_buffer, data + 1 + sizeof(status_packet), data_copy_len);
            status_packet.control_data = g_encode_control_buffer;
            status_packet.control_count = structs_to_copy;

            encoded_payload.packet_type = QLCP_PT_STATUS;
            encoded_payload.payload_data.status = status_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_status(g_encode_buffer, &buffer_len, &status_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    case QLCP_PT_CONFIG:
        {
            qlcp_config_packet config_packet;
            // ensure there is enough data for the memcpy
            if (size - 1 < sizeof(config_packet)) {
                return 0;
            }
            memcpy(&config_packet, data + 1, sizeof(config_packet));

            // copy into the config buffer
            const size_t available_bytes = size - 1 - sizeof(config_packet);
            const size_t data_copy_len = MIN(available_bytes, sizeof(g_encode_config_buffer));

            memcpy(g_encode_config_buffer, data + 1 + sizeof(config_packet), data_copy_len);
            config_packet.config_data = g_encode_config_buffer;
            config_packet.config_data_len = data_copy_len;

            encoded_payload.packet_type = QLCP_PT_CONFIG;
            encoded_payload.payload_data.config = config_packet;

            size_t buffer_len = sizeof(g_encode_buffer);
            if (qlcp_encode_config(g_encode_buffer, &buffer_len, &config_packet) != QLCP_OK) {
                return 0;
            }
        }
        break;
    default:
        return 0;
    }

    qlcp_server_payload decoded_payload = {0};
    qlcp_server_payload_buffers payload_buffers = {
        .sensor_data = g_decode_sensor_buffer,
        .sensor_data_len = sizeof(g_decode_sensor_buffer)/sizeof(g_decode_sensor_buffer[0]),
        .control_data = g_decode_control_buffer,
        .control_data_len = sizeof(g_decode_control_buffer)/sizeof(g_decode_control_buffer[0]),
        .config_data = g_decode_config_buffer,
        .config_data_len = sizeof(g_decode_config_buffer),
    };

    assert(
        qlcp_decode_client_to_server(&decoded_payload, &payload_buffers, g_encode_buffer, sizeof(g_encode_buffer)) == QLCP_OK
    );
    // check that the input payload is equal to the output from the decoder
    assert(
        check_c2s_equality(&encoded_payload, &decoded_payload)
    );

    return 0;
}

// helper to prevent NaN == NaN
static bool compare_float_bytes(float f1, float f2) {
    uint32_t u1, u2;
    memcpy(&u1, &f1, sizeof(uint32_t));
    memcpy(&u2, &f2, sizeof(uint32_t));
    return u1 == u2;
}

// check equality between s2c payloads (must be done manually due to struct padding)
static bool check_s2c_equality(const qlcp_client_payload *p1, const qlcp_client_payload *p2) {
    if (p1->packet_type != p2->packet_type) {
        return false;
    }

    switch (p1->packet_type) {
        case QLCP_PT_ESTOP:
        case QLCP_PT_DISCOVERY:
        case QLCP_PT_STREAM_STOP:
        case QLCP_PT_GET_SINGLE:
        case QLCP_PT_HEARTBEAT:
        case QLCP_PT_STATUS_REQUEST:
            return p1->payload_data.header_only.header.sequence == p2->payload_data.header_only.header.sequence &&
                   p1->payload_data.header_only.header.timestamp_us == p2->payload_data.header_only.header.timestamp_us;
        case QLCP_PT_ACK:
            return p1->payload_data.ack.header.sequence == p2->payload_data.ack.header.sequence &&
                   p1->payload_data.ack.header.timestamp_us == p2->payload_data.ack.header.timestamp_us &&
                   p1->payload_data.ack.ack_packet_type == p2->payload_data.ack.ack_packet_type &&
                   p1->payload_data.ack.ack_sequence == p2->payload_data.ack.ack_sequence;
        case QLCP_PT_NACK:
            return p1->payload_data.nack.header.sequence == p2->payload_data.nack.header.sequence &&
                   p1->payload_data.nack.header.timestamp_us == p2->payload_data.nack.header.timestamp_us &&
                   p1->payload_data.nack.nack_packet_type == p2->payload_data.nack.nack_packet_type &&
                   p1->payload_data.nack.nack_sequence == p2->payload_data.nack.nack_sequence &&
                   p1->payload_data.nack.nack_error_code == p2->payload_data.nack.nack_error_code;
        case QLCP_PT_STREAM_START:
            return p1->payload_data.stream_start.header.sequence == p2->payload_data.stream_start.header.sequence &&
                   p1->payload_data.stream_start.header.timestamp_us == p2->payload_data.stream_start.header.timestamp_us &&
                   p1->payload_data.stream_start.stream_frequency == p2->payload_data.stream_start.stream_frequency;
        case QLCP_PT_CONTROL:
            if (p1->payload_data.control.header.sequence != p2->payload_data.control.header.sequence ||
                p1->payload_data.control.header.timestamp_us != p2->payload_data.control.header.timestamp_us ||
                p1->payload_data.control.control_data.id != p2->payload_data.control.control_data.id ||
                p1->payload_data.control.control_data.type != p2->payload_data.control.control_data.type) {
                return false;
            }
            // Compare only the active union member based on type
            switch (p1->payload_data.control.control_data.type) {
            case QLCP_CONTROL_BOOL:
                if (p1->payload_data.control.control_data.state.control_bool != 
                    p2->payload_data.control.control_data.state.control_bool) return false;
                break;
            case QLCP_CONTROL_UINT32:
                if (p1->payload_data.control.control_data.state.control_uint32 != 
                    p2->payload_data.control.control_data.state.control_uint32) return false;
                break;
            case QLCP_CONTROL_INT32:
                if (p1->payload_data.control.control_data.state.control_int32 != 
                    p2->payload_data.control.control_data.state.control_int32) return false;
                break;
            case QLCP_CONTROL_FLOAT32:
                if (!compare_float_bytes(p1->payload_data.control.control_data.state.control_float32, 
                    p2->payload_data.control.control_data.state.control_float32)) {
                    return false;
                }
                break;
            default:
                   return false;
            }
            return true;
        case QLCP_PT_TIMESYNC_RESP:
            return p1->payload_data.timesync_resp.header.sequence == p2->payload_data.timesync_resp.header.sequence &&
                   p1->payload_data.timesync_resp.header.timestamp_us == p2->payload_data.timesync_resp.header.timestamp_us &&
                   p1->payload_data.timesync_resp.ack_packet_type == p2->payload_data.timesync_resp.ack_packet_type &&
                   p1->payload_data.timesync_resp.ack_sequence == p2->payload_data.timesync_resp.ack_sequence &&
                   p1->payload_data.timesync_resp.t1_echo_us == p2->payload_data.timesync_resp.t1_echo_us &&
                   p1->payload_data.timesync_resp.t2_us == p2->payload_data.timesync_resp.t2_us;
        default:
            return false;
    }
}

// check equality between c2s payloads (must be done manually due to struct padding)
static bool check_c2s_equality(const qlcp_server_payload *p1, const qlcp_server_payload *p2) {
    if (p1->packet_type != p2->packet_type) {
        return false;
    }

    switch (p1->packet_type) {
        case QLCP_PT_TIMESYNC_REQ:
            return p1->payload_data.header_only.header.sequence == p2->payload_data.header_only.header.sequence &&
                   p1->payload_data.header_only.header.timestamp_us == p2->payload_data.header_only.header.timestamp_us;
        case QLCP_PT_ACK:
            return p1->payload_data.ack.header.sequence == p2->payload_data.ack.header.sequence &&
                   p1->payload_data.ack.header.timestamp_us == p2->payload_data.ack.header.timestamp_us &&
                   p1->payload_data.ack.ack_packet_type == p2->payload_data.ack.ack_packet_type &&
                   p1->payload_data.ack.ack_sequence == p2->payload_data.ack.ack_sequence;
        case QLCP_PT_NACK:
            return p1->payload_data.nack.header.sequence == p2->payload_data.nack.header.sequence &&
                   p1->payload_data.nack.header.timestamp_us == p2->payload_data.nack.header.timestamp_us &&
                   p1->payload_data.nack.nack_packet_type == p2->payload_data.nack.nack_packet_type &&
                   p1->payload_data.nack.nack_sequence == p2->payload_data.nack.nack_sequence &&
                   p1->payload_data.nack.nack_error_code == p2->payload_data.nack.nack_error_code;
        case QLCP_PT_DATA:
            if (p1->payload_data.data.header.sequence != p2->payload_data.data.header.sequence ||
                p1->payload_data.data.header.timestamp_us != p2->payload_data.data.header.timestamp_us ||
                p1->payload_data.data.sensor_count != p2->payload_data.data.sensor_count) {
                return false;
            }
            for (size_t i = 0; i < p1->payload_data.data.sensor_count; i++) {
                if (p1->payload_data.data.sensor_data[i].id != p2->payload_data.data.sensor_data[i].id ||
                    !compare_float_bytes(p1->payload_data.data.sensor_data[i].value, p2->payload_data.data.sensor_data[i].value)) {
                    return false;
                }
            }
            return true;
        case QLCP_PT_STATUS:
            if (p1->payload_data.status.header.sequence != p2->payload_data.status.header.sequence ||
                p1->payload_data.status.header.timestamp_us != p2->payload_data.status.header.timestamp_us ||
                p1->payload_data.status.ack_packet_type != p2->payload_data.status.ack_packet_type ||
                p1->payload_data.status.ack_sequence != p2->payload_data.status.ack_sequence ||
                p1->payload_data.status.control_count != p2->payload_data.status.control_count) {
                return false;
            }
            for (size_t i = 0; i < p1->payload_data.status.control_count; i++) {
                if (p1->payload_data.status.control_data[i].id != p2->payload_data.status.control_data[i].id ||
                    p1->payload_data.status.control_data[i].type != p2->payload_data.status.control_data[i].type ||
                    p1->payload_data.status.control_data[i].status != p2->payload_data.status.control_data[i].status) {
                        return false;
                }

                // Compare only the active union member based on type
                switch (p1->payload_data.status.control_data[i].type) {
                case QLCP_CONTROL_BOOL:
                    if (p1->payload_data.status.control_data[i].state.control_bool != 
                        p2->payload_data.status.control_data[i].state.control_bool) return false;
                    break;
                case QLCP_CONTROL_UINT32:
                    if (p1->payload_data.status.control_data[i].state.control_uint32 != 
                        p2->payload_data.status.control_data[i].state.control_uint32) return false;
                case QLCP_CONTROL_INT32:
                    if (p1->payload_data.status.control_data[i].state.control_int32 != 
                        p2->payload_data.status.control_data[i].state.control_int32) return false;
                    break;
                case QLCP_CONTROL_FLOAT32:
                    if (!compare_float_bytes(p1->payload_data.status.control_data[i].state.control_float32, 
                        p2->payload_data.status.control_data[i].state.control_float32)) {
                        return false;
                    }
                    break;
                default:
                       return false;
                }
            }
            return true;
        case QLCP_PT_CONFIG:
            if (p1->payload_data.config.header.sequence != p2->payload_data.config.header.sequence ||
                p1->payload_data.config.header.timestamp_us != p2->payload_data.config.header.timestamp_us ||
                p1->payload_data.config.config_data_len != p2->payload_data.config.config_data_len) return false;
            return (memcmp(p1->payload_data.config.config_data, p2->payload_data.config.config_data, p1->payload_data.config.config_data_len) == 0);
        default:
            return false;
    }
}