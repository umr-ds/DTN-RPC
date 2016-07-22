#include "rpc.h"

// Prepare 2D array of params to 1D array.
char* _rpc_flatten_params (const int paramc, const char **params, const char *delim) {
    // Determine how many chars are there.
    size_t params_size = 0;
    int i;
    for (i = 0; i < paramc; i++){
        params_size = params_size + strlen(params[i]);
    }

    // Iterate over all params and store them in a single string, seperated with '|'
    char *flat_params  = malloc(params_size + paramc + 1);
    for (i = 0; i < paramc; i++) {
        strcat(flat_params, delim);
        strncat(flat_params, params[i], strlen(params[i]));
    }

    return flat_params;
}

// Prepare the payload whith the RPC information.
uint8_t *_rpc_prepare_call_payload (uint8_t *payload, const int paramc, const char *rpc_name, const char *flat_params) {
        // Write the packettype, ...
        write_uint16(&payload[0], RPC_PKT_CALL);
        // ... number of parameters, ...
        write_uint16(&payload[2], (uint16_t) paramc);
        // ... the RPC name ...
        memcpy(&payload[4], rpc_name, strlen(rpc_name));
        // ... and the parameters.
        memcpy(&payload[4 + strlen(rpc_name)], flat_params, strlen(flat_params));

        return payload;
}

// Function for writing arbitary data to a temporary file. CALLER HAS TO REMOVE IT!
size_t _rpc_write_tmp_file (char *file_name, void *content, size_t len) {
    // Create tmp file.
    tmpnam(file_name);
    // Open the file.
    FILE *tmp_file = fopen(file_name, "wb+");
    // Write the data.
    size_t written_size = fwrite(content, 1, len, tmp_file);
    // Close the file.
    fclose(tmp_file);
    return written_size;
}
