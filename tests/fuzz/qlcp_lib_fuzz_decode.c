#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "qlcp_lib.h"

static qlcp_sensor_data g_sensor_buffer[200];
static qlcp_control_data g_control_buffer[200];
static uint8_t g_config_buffer[4096];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    memset(g_sensor_buffer, 0, sizeof(g_sensor_buffer));
    memset(g_control_buffer, 0, sizeof(g_control_buffer));
    memset(g_config_buffer, 0, sizeof(g_config_buffer));
    
    // decode helper fuzzing
    size_t magic_num_idx = 0;
    qlcp_find_magic_num(&magic_num_idx, data, size);

    uint16_t packet_len = 0;
    qlcp_get_packet_len(&packet_len, data, size);

    // client decode fuzzing
    qlcp_client_payload client_payload = {0};
    qlcp_decode_server_to_client(&client_payload, data, size);

    // server decode fuzzing
    qlcp_server_payload server_payload = {0};

    qlcp_server_payload_buffers payload_buffers = {
        .sensor_data = g_sensor_buffer,
        .sensor_data_len = sizeof(g_sensor_buffer)/sizeof(g_sensor_buffer[0]),
        .control_data = g_control_buffer,
        .control_data_len = sizeof(g_control_buffer)/sizeof(g_control_buffer[0]),
        .config_data = g_config_buffer,
        .config_data_len = sizeof(g_config_buffer)
    };

    qlcp_decode_client_to_server(&server_payload, &payload_buffers, data, size);

    return 0; 
}