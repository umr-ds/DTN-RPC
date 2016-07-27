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
    char *flat_params  = calloc(params_size + paramc, sizeof(char));
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
        // Make sure there is a string terminater. Makes it easier to parse on server side.
        memcpy(&payload[4 + strlen(rpc_name) + strlen(flat_params)], "\0", 1);
        
        return payload;
}

// Function for writing arbitary data to a temporary file. CALLER HAS TO REMOVE IT!
ssize_t _rpc_write_tmp_file (char *file_name, void *content, size_t len) {
    // Create a temporary file and open it.
    int tmp_file = mkstemp(file_name);
    // Write the data.
    ssize_t written_size = write(tmp_file, content, len);
    // Close the file.
    close(tmp_file);
    return written_size;
}
