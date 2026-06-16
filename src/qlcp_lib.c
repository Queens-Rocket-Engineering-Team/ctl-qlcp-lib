#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "qlcp_lib.h"

// Private constants
static const uint8_t QLCP_MAGIC_NUM[] = {'Q', 'L', 'C', 'P'};
static_assert(sizeof(QLCP_MAGIC_NUM) == 4, "Magic number is not 4 bytes");

#define QLCP_PROTOCOL_VERSION 3

// C does not guaruntee IEE 754 floats
static_assert(sizeof(float) == 4, "Float size is not 32 bits, protocol assumes IEEE 754 32 bit floats");

// Big-endian helpers
static inline void s_pack_be16(uint8_t *buffer, size_t *offset, uint16_t value);
static inline uint16_t s_unpack_be16(const uint8_t *buffer, size_t *offset);
static inline void s_pack_be32(uint8_t *buffer, size_t *offset, uint32_t value);
static inline uint32_t s_unpack_be32(const uint8_t *buffer, size_t *offset);
static inline void s_pack_be64(uint8_t *buffer, size_t *offset, uint64_t value);
static inline uint64_t s_unpack_be64(const uint8_t *buffer, size_t *offset);

// Internal header struct
typedef struct {
    uint8_t packet_type;
    uint8_t sequence;
    uint16_t packet_length;
    uint64_t timestamp_us;
} qlcp_header_internal;

// Encode a header_data struct into the buffer
static qlcp_lib_ret s_pack_header(uint8_t buffer[], size_t buffer_len, const qlcp_header_internal *header_data) {
    if (buffer == NULL || header_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }

    size_t offset = 0;
    memcpy(buffer, QLCP_MAGIC_NUM, sizeof(QLCP_MAGIC_NUM)); // magic number is QLCP in ascii
    offset += sizeof(QLCP_MAGIC_NUM);

    buffer[offset++] = QLCP_PROTOCOL_VERSION;
    buffer[offset++] = header_data->packet_type;
    buffer[offset++] = header_data->sequence;

    s_pack_be16(buffer, &offset, header_data->packet_length);
    s_pack_be64(buffer, &offset, header_data->timestamp_us);

    return QLCP_OK;
}

// Take a buffer which contains the encoded header, and fill out the header_data struct
static qlcp_lib_ret s_unpack_header(qlcp_header_internal *header_data, const uint8_t buffer[], size_t buffer_len) {
    if (buffer == NULL || header_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }
    size_t offset = 0;
    if (memcmp(buffer, QLCP_MAGIC_NUM, sizeof(QLCP_MAGIC_NUM)) != 0) {
        return QLCP_NO_MAGIC_NUM;
    }
    offset += sizeof(QLCP_MAGIC_NUM);
    if (buffer[offset++] != QLCP_PROTOCOL_VERSION) {
        return QLCP_VERSION_MISMATCH;
    }

    header_data->packet_type = buffer[offset++];
    header_data->sequence = buffer[offset++];

    header_data->packet_length = s_unpack_be16(buffer, &offset);
    if (header_data->packet_length < QLCP_HEADER_SIZE) {
        return QLCP_INVALID_HEADER;
    }

    header_data->timestamp_us = s_unpack_be64(buffer, &offset);

    return QLCP_OK;
}

static inline bool s_is_packet_header_only(qlcp_packet_type packet_type) {
    switch (packet_type) {
    case QLCP_PT_ESTOP:
    case QLCP_PT_DISCOVERY:
    case QLCP_PT_HEARTBEAT:
    case QLCP_PT_STATUS_REQUEST:
    case QLCP_PT_STREAM_STOP:
    case QLCP_PT_GET_SINGLE:
    case QLCP_PT_TIMESYNC_REQ:
        return true;
    default:
        return false;
    }
}

//----------------------------------------------------------
// Encoding implementation
//----------------------------------------------------------

qlcp_lib_ret qlcp_encode_header_only(uint8_t buffer[], size_t *buffer_len, const qlcp_header_only_packet *header_only) {
    if (buffer == NULL || buffer_len == NULL || header_only == NULL) {
        return QLCP_NULL_PTR;
    }
    if (!s_is_packet_header_only(header_only->packet_type)) {
        return QLCP_INVALID_PACKET_TYPE;
    }
    if (*buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_HEADER_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = header_only->packet_type,
        .sequence = header_only->header.sequence,
        .packet_length = QLCP_HEADER_SIZE,
        .timestamp_us = header_only->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }
    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_ack(uint8_t buffer[], size_t *buffer_len, const qlcp_ack_packet *ack) {
    if (buffer == NULL || buffer_len == NULL || ack == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_ACK_PACKET_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_ACK_PACKET_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_ACK,
        .sequence = ack->header.sequence,
        .packet_length = QLCP_ACK_PACKET_SIZE,
        .timestamp_us = ack->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = ack->ack_packet_type;
    buffer[offset++] = ack->ack_sequence;

    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_nack(uint8_t buffer[], size_t *buffer_len, const qlcp_nack_packet *nack) {
    if (buffer == NULL || buffer_len == NULL || nack == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_NACK_PACKET_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_NACK_PACKET_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_NACK,
        .sequence = nack->header.sequence,
        .packet_length = QLCP_NACK_PACKET_SIZE,
        .timestamp_us = nack->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = nack->nack_packet_type;
    buffer[offset++] = nack->nack_sequence;
    buffer[offset++] = nack->nack_error_code;

    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_stream_start(uint8_t buffer[], size_t *buffer_len, const qlcp_stream_start_packet *stream_start) {
    if (buffer == NULL || buffer_len == NULL || stream_start == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_STREAM_START_PACKET_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_STREAM_START_PACKET_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_STREAM_START,
        .sequence = stream_start->header.sequence,
        .packet_length = QLCP_STREAM_START_PACKET_SIZE,
        .timestamp_us = stream_start->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    s_pack_be16(buffer, &offset, stream_start->stream_frequency);

    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_control(uint8_t buffer[], size_t *buffer_len, const qlcp_control_packet *control) {
    if (buffer == NULL || buffer_len == NULL || control == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_CONTROL_PACKET_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_CONTROL_PACKET_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_CONTROL,
        .sequence = control->header.sequence,
        .packet_length = QLCP_CONTROL_PACKET_SIZE,
        .timestamp_us = control->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = control->control_data.id;
    buffer[offset++] = control->control_data.type;

    // Avoid breaking C++ union type punning rules for compatibility
    switch (control->control_data.type) {
    case QLCP_CONTROL_BOOL:
        s_pack_be32(buffer, &offset, (uint32_t)control->control_data.state.control_bool);
        break;
    case QLCP_CONTROL_UINT32:
        s_pack_be32(buffer, &offset, control->control_data.state.control_uint32);
        break;
    case QLCP_CONTROL_INT32:
        s_pack_be32(buffer, &offset, (uint32_t)control->control_data.state.control_int32);
        break;
    case QLCP_CONTROL_FLOAT32:
        {
            // Copy the exact bytes from a float
            uint32_t state_bytes;
            memcpy(&state_bytes, &control->control_data.state.control_float32, sizeof(uint32_t));
            s_pack_be32(buffer, &offset, state_bytes);
        }
        break;
    default:
        return QLCP_INVALID_PACKET;
    }

    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_timesync_resp(uint8_t buffer[], size_t *buffer_len, const qlcp_timesync_resp_packet *timesync_resp) {
    if (buffer == NULL || buffer_len == NULL || timesync_resp == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_TIMESYNC_RESP_PACKET_SIZE) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_TIMESYNC_RESP_PACKET_SIZE;

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_TIMESYNC_RESP,
        .sequence = timesync_resp->header.sequence,
        .packet_length = QLCP_TIMESYNC_RESP_PACKET_SIZE,
        .timestamp_us = timesync_resp->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = timesync_resp->ack_packet_type;
    buffer[offset++] = timesync_resp->ack_sequence;

    s_pack_be64(buffer, &offset, timesync_resp->t1_echo_us);
    s_pack_be64(buffer, &offset, timesync_resp->t2_us);

    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_status(uint8_t buffer[], size_t *buffer_len, const qlcp_status_packet *status) {
    if (buffer == NULL || buffer_len == NULL || status == NULL || status->control_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_STATUS_PACKET_SIZE(status->control_count)) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_STATUS_PACKET_SIZE(status->control_count);

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_STATUS,
        .sequence = status->header.sequence,
        .packet_length = QLCP_STATUS_PACKET_SIZE(status->control_count),
        .timestamp_us = status->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = status->ack_packet_type;
    buffer[offset++] = status->ack_sequence;

    buffer[offset++] = status->control_count;

    for (size_t i = 0; i < status->control_count; i++) {
        buffer[offset++] = status->control_data[i].id;
        buffer[offset++] = status->control_data[i].type;
        // Avoid breaking C++ union type punning rules for compatibility
        switch (status->control_data[i].type) {
        case QLCP_CONTROL_BOOL:
            s_pack_be32(buffer, &offset, (uint32_t)status->control_data[i].state.control_bool);
            break;
        case QLCP_CONTROL_UINT32:
            s_pack_be32(buffer, &offset, status->control_data[i].state.control_uint32);
            break;
        case QLCP_CONTROL_INT32:
            s_pack_be32(buffer, &offset, (uint32_t)status->control_data[i].state.control_int32);
            break;
        case QLCP_CONTROL_FLOAT32:
            {
                // Copy the exact bytes from a float
                uint32_t state_bytes;
                memcpy(&state_bytes, &status->control_data[i].state.control_float32, sizeof(uint32_t));
                s_pack_be32(buffer, &offset, state_bytes);
            }
            break;
        default:
            return QLCP_INVALID_PACKET;
        }
    }
    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_data(uint8_t buffer[], size_t *buffer_len, const qlcp_data_packet *data) {
    if (buffer == NULL || buffer_len == NULL || data == NULL || data->sensor_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_DATA_PACKET_SIZE(data->sensor_count)) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_DATA_PACKET_SIZE(data->sensor_count);

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_DATA,
        .sequence = data->header.sequence,
        .packet_length = QLCP_DATA_PACKET_SIZE(data->sensor_count),
        .timestamp_us = data->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    size_t offset = QLCP_HEADER_SIZE;
    buffer[offset++] = data->sensor_count;

    for (size_t i = 0; i < data->sensor_count; i++) {
        buffer[offset++] = data->sensor_data[i].id;
        buffer[offset++] = data->sensor_data[i].unit;
        // Copy the exact bytes from a float
        uint32_t value_bytes;
        memcpy(&value_bytes, &data->sensor_data[i].value, sizeof(uint32_t));
        s_pack_be32(buffer, &offset, value_bytes);
    }
    return QLCP_OK;
}

qlcp_lib_ret qlcp_encode_config(uint8_t buffer[], size_t *buffer_len, const qlcp_config_packet *config) {
    if (buffer == NULL || buffer_len == NULL || config == NULL || config->config_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (*buffer_len < QLCP_CONFIG_PACKET_SIZE(config->config_data_len)) {
        return QLCP_NO_MEM;
    }
    *buffer_len = QLCP_CONFIG_PACKET_SIZE(config->config_data_len);

    const qlcp_header_internal header_data = {
        .packet_type = QLCP_PT_CONFIG,
        .sequence = config->header.sequence,
        .packet_length = QLCP_CONFIG_PACKET_SIZE(config->config_data_len),
        .timestamp_us = config->header.timestamp_us,
    };

    qlcp_lib_ret ret = s_pack_header(buffer, *buffer_len, &header_data);
    if (ret != QLCP_OK) {
        return ret;
    }

    memcpy(buffer + QLCP_CONFIG_PACKET_SIZE(0), config->config_data, config->config_data_len);

    return QLCP_OK;
}

//----------------------------------------------------------
// Decoding implementation
//----------------------------------------------------------

qlcp_lib_ret qlcp_get_packet_len(uint16_t *packet_len, const uint8_t buffer[], size_t buffer_len) {
    if (buffer == NULL || packet_len == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }
    qlcp_header_internal header_data = {0};

    qlcp_lib_ret ret = s_unpack_header(&header_data, buffer, buffer_len);
    if (ret != QLCP_OK) {
        return ret;
    }

    *packet_len = header_data.packet_length;
    return QLCP_OK;
}

qlcp_lib_ret qlcp_find_magic_num(size_t *magic_num_index, const uint8_t buffer[], size_t buffer_len) {
    if (buffer == NULL || magic_num_index == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < sizeof(QLCP_MAGIC_NUM)) {
        return QLCP_NO_MEM;
    }

    const size_t max_search_limit = buffer_len - sizeof(QLCP_MAGIC_NUM) + 1;
    size_t offset = 0;

    while (offset < max_search_limit) {
        // Search for first byte of magic number
        const uint8_t *pos = memchr(buffer + offset, QLCP_MAGIC_NUM[0], max_search_limit - offset);

        if (pos == NULL) {
            return QLCP_NO_MAGIC_NUM;
        }
        // If first byte found in buffer, memcmp for whole magic number
        if (memcmp(pos, QLCP_MAGIC_NUM, sizeof(QLCP_MAGIC_NUM)) == 0) {
            *magic_num_index = pos - buffer;
            return QLCP_OK;
        }
        // If memcmp is false, move offset to next index after last memchr pos
        offset = (pos - buffer) + 1;
    }
    return QLCP_NO_MAGIC_NUM;
}

qlcp_lib_ret qlcp_decode_client_to_server(qlcp_server_payload *payload, qlcp_server_payload_buffers *payload_buffers, const uint8_t buffer[], size_t buffer_len) {
    if (buffer == NULL || payload == NULL || payload_buffers == NULL) {
        return QLCP_NULL_PTR;
    }
    if (payload_buffers->control_data == NULL || payload_buffers->sensor_data == NULL || payload_buffers->config_data == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }
    // First decode the header to get the packet type
    qlcp_header_internal header_data = {0};

    qlcp_lib_ret ret = s_unpack_header(&header_data, buffer, buffer_len);
    if (ret != QLCP_OK) {
        return ret;
    }
    if (buffer_len < header_data.packet_length) {
        return QLCP_NO_MEM;
    }

    // Tag the packet type
    payload->packet_type = header_data.packet_type;

    switch (header_data.packet_type) {
    case QLCP_PT_TIMESYNC_REQ:
        {
            if (header_data.packet_length != QLCP_HEADER_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.header_only.header.sequence = header_data.sequence;
            payload->payload_data.header_only.header.timestamp_us = header_data.timestamp_us;
            payload->payload_data.header_only.packet_type = header_data.packet_type;
        }
        break;
    case QLCP_PT_ACK:
        {
            if (header_data.packet_length != QLCP_ACK_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.ack.header.sequence = header_data.sequence;
            payload->payload_data.ack.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.ack.ack_packet_type = buffer[offset++];
            payload->payload_data.ack.ack_sequence = buffer[offset++];
        }
        break;
    case QLCP_PT_NACK:
        {
            if (header_data.packet_length != QLCP_NACK_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.nack.header.sequence = header_data.sequence;
            payload->payload_data.nack.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.nack.nack_packet_type = buffer[offset++];
            payload->payload_data.nack.nack_sequence = buffer[offset++];
            payload->payload_data.nack.nack_error_code = buffer[offset++];
        }
        break;
    case QLCP_PT_STATUS:
        {
            if (buffer_len < QLCP_STATUS_PACKET_SIZE(0)) {
                return QLCP_NO_MEM;
            }
            payload->payload_data.status.header.sequence = header_data.sequence;
            payload->payload_data.status.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.status.ack_packet_type = buffer[offset++];
            payload->payload_data.status.ack_sequence = buffer[offset++];

            payload->payload_data.status.control_count = buffer[offset++];
            // Check that there is enough memory in buffers
            const uint8_t control_count = payload->payload_data.status.control_count;
            if (payload_buffers->control_data_len < control_count) {
                return QLCP_NO_MEM;
            }
            if (header_data.packet_length != QLCP_STATUS_PACKET_SIZE(control_count)) {
                return QLCP_LEN_MISMATCH;
            }

            payload->payload_data.status.control_data = payload_buffers->control_data;

            for (size_t i = 0; i < control_count; i++) {
                payload_buffers->control_data[i].id = buffer[offset++];
                payload_buffers->control_data[i].type = buffer[offset++];
                // Avoid breaking C++ union type punning rules for compatibility
                switch (payload_buffers->control_data[i].type) {
                case QLCP_CONTROL_BOOL:
                    payload_buffers->control_data[i].state.control_bool = (uint8_t)s_unpack_be32(buffer, &offset);
                    break;
                case QLCP_CONTROL_UINT32:
                    payload_buffers->control_data[i].state.control_uint32 = s_unpack_be32(buffer, &offset);
                    break;
                case QLCP_CONTROL_INT32:
                    payload_buffers->control_data[i].state.control_int32 = (uint32_t)s_unpack_be32(buffer, &offset);
                    break;
                case QLCP_CONTROL_FLOAT32:
                    {
                        const uint32_t state_bytes = s_unpack_be32(buffer, &offset);
                        memcpy(&payload_buffers->control_data[i].state.control_float32, &state_bytes, sizeof(uint32_t));
                    }
                    break;
                default:
                    return QLCP_INVALID_PACKET;
                }
            }
        }
        break;
    case QLCP_PT_DATA:
        {
            if (buffer_len < QLCP_DATA_PACKET_SIZE(0)) {
                return QLCP_NO_MEM;
            }
            payload->payload_data.data.header.sequence = header_data.sequence;
            payload->payload_data.data.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.data.sensor_count = buffer[offset++];
            // Check that there is enough memory in buffers
            const uint8_t sensor_count = payload->payload_data.data.sensor_count;
            if (payload_buffers->sensor_data_len < sensor_count) {
                return QLCP_NO_MEM;
            }
            if (header_data.packet_length != QLCP_DATA_PACKET_SIZE(sensor_count)) {
                return QLCP_LEN_MISMATCH;
            }

            payload->payload_data.data.sensor_data = payload_buffers->sensor_data;

            for (size_t i = 0; i < sensor_count; i++) {
                payload_buffers->sensor_data[i].id = buffer[offset++];
                payload_buffers->sensor_data[i].unit = buffer[offset++];
                // Bytes to float
                const uint32_t value_bytes = s_unpack_be32(buffer, &offset);
                memcpy(&payload_buffers->sensor_data[i].value, &value_bytes, sizeof(uint32_t));
            }
        }
        break;
    case QLCP_PT_CONFIG:
        {
            if (header_data.packet_length < QLCP_CONFIG_PACKET_SIZE(0)) {
                return QLCP_LEN_MISMATCH;
            }
            const uint16_t config_len = header_data.packet_length - QLCP_CONFIG_PACKET_SIZE(0);
            // Check that there is enough memory in buffers
            if (payload_buffers->config_data_len < config_len) {
                return QLCP_NO_MEM;
            }
            payload->payload_data.config.header.sequence = header_data.sequence;
            payload->payload_data.config.header.timestamp_us = header_data.timestamp_us;

            payload->payload_data.config.config_data_len = config_len;
            payload->payload_data.config.config_data = payload_buffers->config_data;

            memcpy(payload_buffers->config_data, buffer + QLCP_CONFIG_PACKET_SIZE(0), config_len);
        }
        break;
    default:
        return QLCP_INVALID_PACKET_TYPE;
    }

    return QLCP_OK;
}

qlcp_lib_ret qlcp_decode_server_to_client(qlcp_client_payload *payload, const uint8_t buffer[], size_t buffer_len) {
    if (buffer == NULL || payload == NULL) {
        return QLCP_NULL_PTR;
    }
    if (buffer_len < QLCP_HEADER_SIZE) {
        return QLCP_NO_MEM;
    }
    // First decode the header to get the packet type
    qlcp_header_internal header_data = {0};

    qlcp_lib_ret ret = s_unpack_header(&header_data, buffer, buffer_len);
    if (ret != QLCP_OK) {
        return ret;
    }
    if (buffer_len < header_data.packet_length) {
        return QLCP_NO_MEM;
    }

    // Tag the packet type
    payload->packet_type = header_data.packet_type;

    switch (header_data.packet_type) {
    case QLCP_PT_ESTOP:
    case QLCP_PT_DISCOVERY:
    case QLCP_PT_STREAM_STOP:
    case QLCP_PT_GET_SINGLE:
    case QLCP_PT_HEARTBEAT:
    case QLCP_PT_STATUS_REQUEST:
        {
            if (header_data.packet_length != QLCP_HEADER_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.header_only.header.sequence = header_data.sequence;
            payload->payload_data.header_only.header.timestamp_us = header_data.timestamp_us;
            payload->payload_data.header_only.packet_type = header_data.packet_type;
        }
        break;
    case QLCP_PT_ACK:
        {
            if (header_data.packet_length != QLCP_ACK_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.ack.header.sequence = header_data.sequence;
            payload->payload_data.ack.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.ack.ack_packet_type = buffer[offset++];
            payload->payload_data.ack.ack_sequence = buffer[offset++];
        }
        break;
    case QLCP_PT_NACK:
        {
            if (header_data.packet_length != QLCP_NACK_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.nack.header.sequence = header_data.sequence;
            payload->payload_data.nack.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.nack.nack_packet_type = buffer[offset++];
            payload->payload_data.nack.nack_sequence = buffer[offset++];
            payload->payload_data.nack.nack_error_code = buffer[offset++];
        }
        break;
    case QLCP_PT_STREAM_START:
        {
            if (header_data.packet_length != QLCP_STREAM_START_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.stream_start.header.sequence = header_data.sequence;
            payload->payload_data.stream_start.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.stream_start.stream_frequency = s_unpack_be16(buffer, &offset);
        }
        break;
    case QLCP_PT_CONTROL:
        {
            if (header_data.packet_length != QLCP_CONTROL_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.control.header.sequence = header_data.sequence;
            payload->payload_data.control.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.control.control_data.id = buffer[offset++];
            payload->payload_data.control.control_data.type = buffer[offset++];
            // Avoid breaking C++ union type punning rules for compatibility
            switch (payload->payload_data.control.control_data.type) {
            case QLCP_CONTROL_BOOL:
                payload->payload_data.control.control_data.state.control_bool = (uint8_t)s_unpack_be32(buffer, &offset);
                break;
            case QLCP_CONTROL_UINT32:
                payload->payload_data.control.control_data.state.control_uint32 = s_unpack_be32(buffer, &offset);
                break;
            case QLCP_CONTROL_INT32:
                payload->payload_data.control.control_data.state.control_int32 = (uint32_t)s_unpack_be32(buffer, &offset);
                break;
            case QLCP_CONTROL_FLOAT32:
                {
                    const uint32_t state_bytes = s_unpack_be32(buffer, &offset);
                    memcpy(&payload->payload_data.control.control_data.state.control_float32, &state_bytes, sizeof(uint32_t));
                }
                break;
            default:
                return QLCP_INVALID_PACKET;
            }
        }
        break;
    case QLCP_PT_TIMESYNC_RESP:
        {
            if (header_data.packet_length != QLCP_TIMESYNC_RESP_PACKET_SIZE) {
                return QLCP_LEN_MISMATCH;
            }
            payload->payload_data.timesync_resp.header.sequence = header_data.sequence;
            payload->payload_data.timesync_resp.header.timestamp_us = header_data.timestamp_us;

            size_t offset = QLCP_HEADER_SIZE;
            payload->payload_data.timesync_resp.ack_packet_type = buffer[offset++];
            payload->payload_data.timesync_resp.ack_sequence = buffer[offset++];

            payload->payload_data.timesync_resp.t1_echo_us = s_unpack_be64(buffer, &offset);
            payload->payload_data.timesync_resp.t2_us = s_unpack_be64(buffer, &offset);
        }
        break;
    default:
        return QLCP_INVALID_PACKET_TYPE;
    }

    return QLCP_OK;
}

//----------------------------------------------------------
// Big-endian helpers
//----------------------------------------------------------

static inline void s_pack_be16(uint8_t *buffer, size_t *offset, uint16_t value) {
    buffer[*offset + 0] = (uint8_t)(value >> 8);
    buffer[*offset + 1] = (uint8_t)(value);
    *offset += 2;
}

static inline uint16_t s_unpack_be16(const uint8_t *buffer, size_t *offset) {
    uint16_t value = ((uint16_t)buffer[*offset + 0] << 8) |
                     ((uint16_t)buffer[*offset + 1]);
    *offset += 2;
    return value;
}

static inline void s_pack_be32(uint8_t *buffer, size_t *offset, uint32_t value) {
    buffer[*offset + 0] = (uint8_t)(value >> 24);
    buffer[*offset + 1] = (uint8_t)(value >> 16);
    buffer[*offset + 2] = (uint8_t)(value >> 8);
    buffer[*offset + 3] = (uint8_t)(value);
    *offset += 4;
}

static inline uint32_t s_unpack_be32(const uint8_t *buffer, size_t *offset) {
    uint32_t value = ((uint32_t)buffer[*offset + 0] << 24) |
                     ((uint32_t)buffer[*offset + 1] << 16) |
                     ((uint32_t)buffer[*offset + 2] << 8) |
                     ((uint32_t)buffer[*offset + 3]);
    *offset += 4;
    return value;
}

static inline void s_pack_be64(uint8_t *buffer, size_t *offset, uint64_t value) {
    buffer[*offset + 0] = (uint8_t)(value >> 56);
    buffer[*offset + 1] = (uint8_t)(value >> 48);
    buffer[*offset + 2] = (uint8_t)(value >> 40);
    buffer[*offset + 3] = (uint8_t)(value >> 32);
    buffer[*offset + 4] = (uint8_t)(value >> 24);
    buffer[*offset + 5] = (uint8_t)(value >> 16);
    buffer[*offset + 6] = (uint8_t)(value >> 8);
    buffer[*offset + 7] = (uint8_t)(value);
    *offset += 8;
}

static inline uint64_t s_unpack_be64(const uint8_t *buffer, size_t *offset) {
    uint64_t value = ((uint64_t)buffer[*offset + 0] << 56) |
                     ((uint64_t)buffer[*offset + 1] << 48) |
                     ((uint64_t)buffer[*offset + 2] << 40) |
                     ((uint64_t)buffer[*offset + 3] << 32) |
                     ((uint64_t)buffer[*offset + 4] << 24) |
                     ((uint64_t)buffer[*offset + 5] << 16) |
                     ((uint64_t)buffer[*offset + 6] << 8) |
                     ((uint64_t)buffer[*offset + 7]);
    *offset += 8;
    return value;
}