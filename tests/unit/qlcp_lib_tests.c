#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "qlcp_lib.h"

const uint8_t sequence = UINT8_MAX;
const uint64_t timestamp_us = UINT64_MAX;

// global test state
static uint8_t g_buffer[4096];
static qlcp_client_payload g_client_payload;
static qlcp_server_payload g_server_payload;

static qlcp_sensor_data g_sensor_buffer[200];
static qlcp_control_data g_control_buffer[200];
static uint8_t g_config_buffer[4096];
static qlcp_server_payload_buffers g_server_payload_buffers;

void setUp(void) {
    // zero all buffers before every test
    memset(g_buffer, 0, sizeof(g_buffer));
    memset(&g_client_payload, 0, sizeof(g_client_payload));
    memset(&g_server_payload, 0, sizeof(g_server_payload));
    
    memset(g_sensor_buffer, 0, sizeof(g_sensor_buffer));
    memset(g_control_buffer, 0, sizeof(g_control_buffer));
    memset(g_config_buffer, 0, sizeof(g_config_buffer));

    g_server_payload_buffers.sensor_data = g_sensor_buffer;
    g_server_payload_buffers.sensor_data_len = sizeof(g_sensor_buffer)/sizeof(g_sensor_buffer[0]);
    
    g_server_payload_buffers.control_data = g_control_buffer;
    g_server_payload_buffers.control_data_len = sizeof(g_control_buffer)/sizeof(g_control_buffer[0]);
    
    g_server_payload_buffers.config_data = g_config_buffer;
    g_server_payload_buffers.config_data_len = sizeof(g_config_buffer);
}

void tearDown(void) {

}

typedef struct {
    qlcp_packet_type packet_type;
    qlcp_lib_ret expected_encode_ret;
    qlcp_lib_ret expected_server_to_client_decode_ret;
    qlcp_lib_ret expected_client_to_server_decode_ret;
} qlcp_test_case_t;

// ---------------------------------------------------------
// Negative decode helper
// ---------------------------------------------------------
void run_negative_decode_tests(const uint8_t valid_buffer[], size_t valid_len, bool is_client_to_server) {
    uint8_t bad_buffer[1024];
    qlcp_lib_ret ret;

    // test null pointers
    if (is_client_to_server) {
        TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_decode_client_to_server(NULL, &g_server_payload_buffers, valid_buffer, valid_len));
        TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_decode_client_to_server(&g_server_payload, NULL, valid_buffer, valid_len));
        TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, NULL, valid_len));
    } else {
        TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_decode_server_to_client(NULL, valid_buffer, valid_len));
        TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_decode_server_to_client(&g_client_payload, NULL, valid_len));
    }

    // test invalid magic number
    memcpy(bad_buffer, valid_buffer, valid_len);
    bad_buffer[0] = 'X'; 
    ret = is_client_to_server ? 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, bad_buffer, valid_len) :
        qlcp_decode_server_to_client(&g_client_payload, bad_buffer, valid_len);
    TEST_ASSERT_EQUAL_MESSAGE(QLCP_NO_MAGIC_NUM, ret, "Failed to return QLCP_NO_MAGIC_NUM");

    // test mismatched protocol version
    memcpy(bad_buffer, valid_buffer, valid_len);
    bad_buffer[4] = 99; 
    ret = is_client_to_server ? 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, bad_buffer, valid_len) :
        qlcp_decode_server_to_client(&g_client_payload, bad_buffer, valid_len);
    TEST_ASSERT_EQUAL_MESSAGE(QLCP_VERSION_MISMATCH, ret, "Failed to return QLCP_VERSION_MISMATCH");

    // test truncated buffers
    for (size_t short_len = 0; short_len < valid_len; short_len++) {
        ret = is_client_to_server ? 
            qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, valid_buffer, short_len) :
            qlcp_decode_server_to_client(&g_client_payload, valid_buffer, short_len);
        TEST_ASSERT_TRUE_MESSAGE(ret == QLCP_NO_MEM || ret == QLCP_LEN_MISMATCH, "Failed to catch truncated packet");
    }
}

// ---------------------------------------------------------
// Decode helper
// ---------------------------------------------------------
bool run_decode_generic(uint8_t buffer[], size_t buffer_len, qlcp_test_case_t test_case) {
    qlcp_lib_ret s2c_ret = qlcp_decode_server_to_client(&g_client_payload, buffer, buffer_len);
    TEST_ASSERT_EQUAL_INT_MESSAGE(test_case.expected_server_to_client_decode_ret, s2c_ret, "S2C decode return does not match");

    qlcp_lib_ret c2s_ret = qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, buffer, buffer_len);
    TEST_ASSERT_EQUAL_INT_MESSAGE(test_case.expected_client_to_server_decode_ret, c2s_ret, "C2S decode return does not match");

    if (s2c_ret == QLCP_OK) {
        TEST_ASSERT_EQUAL_UINT8(test_case.packet_type, g_client_payload.packet_type);
        TEST_ASSERT_EQUAL_UINT8(sequence, g_client_payload.payload_data.header_only.header.sequence);
        TEST_ASSERT_EQUAL_UINT64(timestamp_us, g_client_payload.payload_data.header_only.header.timestamp_us);
        run_negative_decode_tests(buffer, buffer_len, false);
    }
    if (c2s_ret == QLCP_OK) {
        TEST_ASSERT_EQUAL_UINT8(test_case.packet_type, g_server_payload.packet_type);
        TEST_ASSERT_EQUAL_UINT8(sequence, g_server_payload.payload_data.header_only.header.sequence);
        TEST_ASSERT_EQUAL_UINT64(timestamp_us, g_server_payload.payload_data.header_only.header.timestamp_us);
        run_negative_decode_tests(buffer, buffer_len, true);
    }
    return (s2c_ret == test_case.expected_server_to_client_decode_ret && c2s_ret == test_case.expected_client_to_server_decode_ret);
}

// ---------------------------------------------------------
// Packet-specific tests
// ---------------------------------------------------------
void test_header_only_encode_decode(void) {
    const qlcp_test_case_t test_cases[] = {
        {QLCP_PT_ESTOP, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_DISCOVERY, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_HEARTBEAT, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_STATUS_REQUEST, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_STREAM_STOP, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_GET_SINGLE, QLCP_OK, QLCP_OK, QLCP_INVALID_PACKET_TYPE},
        {QLCP_PT_TIMESYNC_REQ, QLCP_OK, QLCP_INVALID_PACKET_TYPE, QLCP_OK},
        {QLCP_PT_CONTROL, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_STREAM_START, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_TIMESYNC_RESP, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_CONFIG, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_DATA, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_STATUS, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_ACK, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
        {QLCP_PT_NACK, QLCP_INVALID_PACKET_TYPE, QLCP_OK, QLCP_OK},
    }; 

    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++) {
        qlcp_header_only_packet header_only = {
            .header = {
                .sequence = sequence,
                .timestamp_us = timestamp_us
            },
            .packet_type = test_cases[i].packet_type,
        };

        size_t buffer_len = sizeof(g_buffer);
        qlcp_lib_ret encode_ret = qlcp_encode_header_only(g_buffer, &buffer_len, &header_only);
        
        TEST_ASSERT_EQUAL_INT(test_cases[i].expected_encode_ret, encode_ret);
        if (encode_ret == QLCP_OK) {
            run_decode_generic(g_buffer, buffer_len, test_cases[i]);
        }
    }
}

void test_ack_encode_decode(void) {
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_ACK,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_OK,
        .expected_client_to_server_decode_ret = QLCP_OK,
    };

    qlcp_ack_packet ack = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .ack_packet_type = QLCP_PT_HEARTBEAT,
        .ack_sequence = 200,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_ack(g_buffer, &buffer_len, &ack));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(ack.ack_packet_type, g_client_payload.payload_data.ack.ack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(ack.ack_sequence, g_client_payload.payload_data.ack.ack_sequence);
        
        TEST_ASSERT_EQUAL_UINT8(ack.ack_packet_type, g_server_payload.payload_data.ack.ack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(ack.ack_sequence, g_server_payload.payload_data.ack.ack_sequence);
    }
}

void test_nack_encode_decode(void) {
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_NACK,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_OK,
        .expected_client_to_server_decode_ret = QLCP_OK,
    };

    qlcp_nack_packet nack = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .nack_packet_type = QLCP_PT_HEARTBEAT,
        .nack_sequence = 200,
        .nack_error_code = QLCP_ERR_BUSY,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_nack(g_buffer, &buffer_len, &nack));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(nack.nack_packet_type, g_client_payload.payload_data.nack.nack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(nack.nack_sequence, g_client_payload.payload_data.nack.nack_sequence);
        TEST_ASSERT_EQUAL_UINT8(nack.nack_error_code, g_client_payload.payload_data.nack.nack_error_code);
        
        TEST_ASSERT_EQUAL_UINT8(nack.nack_packet_type, g_server_payload.payload_data.nack.nack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(nack.nack_sequence, g_server_payload.payload_data.nack.nack_sequence);
        TEST_ASSERT_EQUAL_UINT8(nack.nack_error_code, g_server_payload.payload_data.nack.nack_error_code);
    }
}

void test_control_encode_decode(void) {
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_CONTROL,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_OK,
        .expected_client_to_server_decode_ret = QLCP_INVALID_PACKET_TYPE,
    };

    qlcp_control_packet control = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .control_id = 45,
        .control_state = QLCP_CS_CLOSED,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_control(g_buffer, &buffer_len, &control));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(control.control_id, g_client_payload.payload_data.control.control_id);
        TEST_ASSERT_EQUAL_UINT8(control.control_state, g_client_payload.payload_data.control.control_state);
    }
}

void test_stream_start_encode_decode(void) {
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_STREAM_START,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_OK,
        .expected_client_to_server_decode_ret = QLCP_INVALID_PACKET_TYPE,
    };

    qlcp_stream_start_packet stream_start = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .stream_frequency = 12345,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_stream_start(g_buffer, &buffer_len, &stream_start));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT16(stream_start.stream_frequency, g_client_payload.payload_data.stream_start.stream_frequency);
    }
}

void test_timesync_resp_encode_decode(void) {
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_TIMESYNC_RESP,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_OK,
        .expected_client_to_server_decode_ret = QLCP_INVALID_PACKET_TYPE,
    };

    qlcp_timesync_resp_packet timesync_resp = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .ack_packet_type = QLCP_PT_TIMESYNC_REQ,
        .ack_sequence = 255,
        .t1_echo_us = 472937522894,
        .t2_us = 843282478287,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_timesync_resp(g_buffer, &buffer_len, &timesync_resp));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(timesync_resp.ack_packet_type, g_client_payload.payload_data.timesync_resp.ack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(timesync_resp.ack_sequence, g_client_payload.payload_data.timesync_resp.ack_sequence);

        TEST_ASSERT_EQUAL_UINT64(timesync_resp.t1_echo_us, g_client_payload.payload_data.timesync_resp.t1_echo_us);
        TEST_ASSERT_EQUAL_UINT64(timesync_resp.t2_us, g_client_payload.payload_data.timesync_resp.t2_us);
    }
}

void test_config_encode_decode(void) {
    const uint8_t dummy_json[] = "{\"device_name\": \"DEVICE_NAME\"}";
    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_CONFIG,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_INVALID_PACKET_TYPE,
        .expected_client_to_server_decode_ret = QLCP_OK,
    };

    qlcp_config_packet config = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .config_data = dummy_json,
        .config_data_len = sizeof(dummy_json) - 1, 
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_config(g_buffer, &buffer_len, &config));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_MEMORY(dummy_json, g_server_payload.payload_data.config.config_data, sizeof(dummy_json) - 1);
    }
}

void test_data_encode_decode(void) {
    const qlcp_sensor_data sensor_data[] = {
        {
            .sensor_id = 0,
            .unit = QLCP_UNIT_HERTZ,
            .value = __FLT_MAX__
        },
        {
            .sensor_id = 1,
            .unit = QLCP_UNIT_AMPS,
            .value = __FLT_MIN__
        },
    };

    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_DATA,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_INVALID_PACKET_TYPE,
        .expected_client_to_server_decode_ret = QLCP_OK,
    };

    qlcp_data_packet data = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .sensor_count = sizeof(sensor_data)/sizeof(sensor_data[0]),
        .sensor_data = sensor_data,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_data(g_buffer, &buffer_len, &data));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(data.sensor_count, g_server_payload.payload_data.data.sensor_count);
        for (size_t i = 0; i < data.sensor_count; i++) {
            TEST_ASSERT_EQUAL_UINT8(sensor_data[i].unit, g_server_payload.payload_data.data.sensor_data[i].unit);
            TEST_ASSERT_EQUAL_FLOAT(sensor_data[i].value, g_server_payload.payload_data.data.sensor_data[i].value);
        }
    }
}

void test_status_encode_decode(void) {
    const qlcp_control_data control_data[] = {
        {
            .control_id = 0,
            .control_state = QLCP_CS_CLOSED
        },
        {
            .control_id = 1,
            .control_state = QLCP_CS_OPEN
        },
    };

    qlcp_test_case_t test_case = {
        .packet_type = QLCP_PT_STATUS,
        .expected_encode_ret = QLCP_OK,
        .expected_server_to_client_decode_ret = QLCP_INVALID_PACKET_TYPE,
        .expected_client_to_server_decode_ret = QLCP_OK,
    };

    qlcp_status_packet status = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us
        },
        .ack_packet_type = QLCP_PT_STATUS_REQUEST,
        .ack_sequence = 200,
        .device_status = QLCP_DS_ACTIVE,
        .control_count = sizeof(control_data)/sizeof(control_data[0]),
        .control_data = control_data,
    };

    size_t buffer_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL_INT(QLCP_OK, qlcp_encode_status(g_buffer, &buffer_len, &status));

    if (run_decode_generic(g_buffer, buffer_len, test_case)) {
        TEST_ASSERT_EQUAL_UINT8(status.ack_packet_type, g_server_payload.payload_data.status.ack_packet_type);
        TEST_ASSERT_EQUAL_UINT8(status.ack_sequence, g_server_payload.payload_data.status.ack_sequence);
        TEST_ASSERT_EQUAL_UINT8(status.device_status, g_server_payload.payload_data.status.device_status);
        TEST_ASSERT_EQUAL_UINT8(status.control_count, g_server_payload.payload_data.status.control_count);
        for (size_t i = 0; i < status.control_count; i++) {
            TEST_ASSERT_EQUAL_UINT8(control_data[i].control_id, g_server_payload.payload_data.status.control_data[i].control_id);
            TEST_ASSERT_EQUAL_FLOAT(control_data[i].control_state, g_server_payload.payload_data.status.control_data[i].control_state);
        }
    }
}

// ---------------------------------------------------------
// Array bounds/overflows
// ---------------------------------------------------------
void test_encode_bounds(void) {
    qlcp_header_only_packet header = {
        .header = {
            .sequence = sequence,
            .timestamp_us = timestamp_us,
        },
        .packet_type = QLCP_PT_HEARTBEAT,
    };

    size_t too_small_len = QLCP_HEADER_SIZE - 1;
    
    TEST_ASSERT_EQUAL(QLCP_NO_MEM, qlcp_encode_header_only(g_buffer, &too_small_len, &header));

    // test NULL handling
    size_t good_len = sizeof(g_buffer);
    TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_encode_header_only(NULL, &good_len, &header));
    TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_encode_header_only(g_buffer, NULL, &header));
    TEST_ASSERT_EQUAL(QLCP_NULL_PTR, qlcp_encode_header_only(g_buffer, &good_len, NULL));
}

void test_malicious_packets(void) {
    uint8_t mal_buffer[1024];
    
    // DATA packet claims 250 sensors (server max buffer is 200)
    memset(mal_buffer, 0, sizeof(mal_buffer));
    memcpy(mal_buffer, "QLCP", 4);
    mal_buffer[4] = 3;               // version
    mal_buffer[5] = QLCP_PT_DATA;    // packet_type
    mal_buffer[6] = 1;               // seq
    uint16_t data_len = QLCP_DATA_PACKET_SIZE(250); 
    mal_buffer[7] = (uint8_t)(data_len >> 8);
    mal_buffer[8] = (uint8_t)(data_len);
    mal_buffer[QLCP_HEADER_SIZE + 0] = 250; // sensor_count byte at QLCP_HEADER_SIZE + 0
    
    TEST_ASSERT_EQUAL_MESSAGE(
        QLCP_NO_MEM, 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, mal_buffer, sizeof(mal_buffer)),
        "Failed to block DATA buffer overflow"
    );

    // DATA packet claims 5 sensors, but length in header claims smaller
    mal_buffer[QLCP_HEADER_SIZE + 0] = 5;
    uint16_t data_mismatched_len = QLCP_DATA_PACKET_SIZE(2);
    mal_buffer[7] = (uint8_t)(data_mismatched_len >> 8);
    mal_buffer[8] = (uint8_t)(data_mismatched_len);
    
    TEST_ASSERT_EQUAL_MESSAGE(
        QLCP_LEN_MISMATCH, 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, mal_buffer, sizeof(mal_buffer)),
        "Failed to catch DATA length mismatch"
    );

    // STATUS packet claims 250 controls (server max buffer is 200)
    memset(mal_buffer, 0, sizeof(mal_buffer));
    memcpy(mal_buffer, "QLCP", 4);
    mal_buffer[4] = 3;
    mal_buffer[5] = QLCP_PT_STATUS;
    uint16_t status_len = QLCP_STATUS_PACKET_SIZE(250);
    mal_buffer[7] = (uint8_t)(status_len >> 8);
    mal_buffer[8] = (uint8_t)(status_len);
    mal_buffer[QLCP_HEADER_SIZE + 3] = 250; // control_count byte at QLCP_HEADER_SIZE + 3
    
    TEST_ASSERT_EQUAL_MESSAGE(
        QLCP_NO_MEM, 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, mal_buffer, sizeof(mal_buffer)),
        "Failed to block STATUS buffer overflow"
    );

    // STATUS packet claims 5 controls, but length in header claims smaller
    mal_buffer[QLCP_HEADER_SIZE + 3] = 5;
    uint16_t status_mismatched_len = QLCP_STATUS_PACKET_SIZE(2);
    mal_buffer[7] = (uint8_t)(status_mismatched_len >> 8);
    mal_buffer[8] = (uint8_t)(status_mismatched_len);
    
    TEST_ASSERT_EQUAL_MESSAGE(
        QLCP_LEN_MISMATCH, 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, mal_buffer, sizeof(mal_buffer)),
        "Failed to catch STATUS length mismatch"
    );

    // CONFIG packet claims a size larger than config_buffer (4096)
    memset(mal_buffer, 0, sizeof(mal_buffer));
    memcpy(mal_buffer, "QLCP", 4);
    mal_buffer[4] = 3;
    mal_buffer[5] = QLCP_PT_CONFIG;
    uint16_t config_len = QLCP_CONFIG_PACKET_SIZE(4097); // 1 byte larger than max
    mal_buffer[7] = (uint8_t)(config_len >> 8);
    mal_buffer[8] = (uint8_t)(config_len);
    
    TEST_ASSERT_EQUAL_MESSAGE(
        QLCP_NO_MEM, 
        qlcp_decode_client_to_server(&g_server_payload, &g_server_payload_buffers, mal_buffer, sizeof(mal_buffer)),
        "Failed to block CONFIG buffer overflow"
    );
}

// ---------------------------------------------------------
// Helper function tests
// ---------------------------------------------------------
void test_helper_functions(void) {
    uint8_t buffer[64];
    size_t out_index = 0;
    uint16_t out_len = 0;

    // qlcp_find_magic_num tests
    
    // magic number not present
    memset(buffer, 'X', sizeof(buffer));
    TEST_ASSERT_EQUAL(QLCP_NO_MAGIC_NUM, qlcp_find_magic_num(&out_index, buffer, sizeof(buffer)));
    
    // magic number at the first index
    memcpy(buffer, "QLCP", 4);
    TEST_ASSERT_EQUAL(QLCP_OK, qlcp_find_magic_num(&out_index, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT32(0, out_index);
    
    // magic number in the middle
    memset(buffer, 'X', sizeof(buffer));
    memcpy(buffer + 15, "QLCP", 4);
    TEST_ASSERT_EQUAL(QLCP_OK, qlcp_find_magic_num(&out_index, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT32(15, out_index);

    // magic number starts at the third last index (no space)
    memset(buffer, 'X', sizeof(buffer));
    memcpy(buffer + (sizeof(buffer) - 1 - 3), "QLC", 3);
    TEST_ASSERT_EQUAL(QLCP_NO_MAGIC_NUM, qlcp_find_magic_num(&out_index, buffer, sizeof(buffer)));

    // buffer too small
    TEST_ASSERT_EQUAL(QLCP_NO_MEM, qlcp_find_magic_num(&out_index, buffer, 3));

    // qlcp_get_packet_len tests
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "QLCP", 4);
    buffer[4] = 3;    // version
    buffer[7] = 0x12; // length MSB
    buffer[8] = 0x34; // length LSB

    TEST_ASSERT_EQUAL(QLCP_OK, qlcp_get_packet_len(&out_len, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT16(0x1234, out_len);

    // buffer too small for header
    TEST_ASSERT_EQUAL(QLCP_NO_MEM, qlcp_get_packet_len(&out_len, buffer, QLCP_HEADER_SIZE - 1));
}

// ---------------------------------------------------------
// Unity testing
// ---------------------------------------------------------
int32_t main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_header_only_encode_decode);
    RUN_TEST(test_ack_encode_decode);
    RUN_TEST(test_nack_encode_decode);
    RUN_TEST(test_control_encode_decode);
    RUN_TEST(test_stream_start_encode_decode);
    RUN_TEST(test_timesync_resp_encode_decode);
    RUN_TEST(test_config_encode_decode);
    RUN_TEST(test_data_encode_decode);
    RUN_TEST(test_status_encode_decode);

    RUN_TEST(test_helper_functions);

    RUN_TEST(test_encode_bounds);
    RUN_TEST(test_malicious_packets);

    return UNITY_END();
}