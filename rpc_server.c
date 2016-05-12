#include "rpc.h"

#define RPC_CONF_FILENAME "rpc.conf"

// This var is to have some global server status to handle RPCs in an appropriate way.
// 0: everything okay.
// 1: RPC not offered.
// 2: Could not execute RPC.
int status = 0;

int running = 0;

int mdp_fd;

// Function to check, if a RPC is offered by this server.
// Load and parse the rpc.conf file.
int rpc_check_offered (struct rpc_procedure *rp) {
    DEBUGF(rpc, "Checking, if %s is offered.", rp->name);
    // Build the path to the rpc.conf file and open it.
    static char path[strlen(SYSCONFDIR) + 9 + strlen("rpc.conf")] = "";
    FORMF_SERVAL_ETC_PATH(path, RPC_CONF_FILENAME);
    FILE *conf_file = fopen(path, "r");
    
    char *line = NULL;
    size_t len = 0;
    int ret = -1;
    
    // Read the file line by line.
    // TODO: UNIQUE RPC NAME
    while (getline(&line, &len, conf_file) != -1) {
        // Split the line at the first space to get the return type.
        char *rt = strtok(line, " ");
        // Split the line at the second space to get the name.
        char *tok = strtok(NULL, " ");
        
        // If the name matches with the received name, this server offers
        // the desired procedure.
        if (strncmp(tok, rp->name, strlen(tok)) == 0) {
            // Sideeffect: parse the return type and store it in the rp struct
            rp->return_type = malloc(strlen(rt));
            strncpy(rp->return_type, rt, strlen(rt));
            ret = 0;
            break;
        }
    }
    
    // Cleanup.
    fclose(conf_file);
    if (line) {
        free(line);
    }
    
    return ret;
}

// Function to parse the received payload.
struct rpc_procedure rpc_parse_call (const uint8_t *payload, size_t len) {
    DEBUG(rpc, "Parsing call.");
    // Create a new rp struct.
    struct rpc_procedure rp;
    
    // Parse the parameter count.
    rp.paramc = read_uint16(&payload[2]);
    
    // Cast the payload starting at byte 5 to string.
    // The first 4 bytes are for packet type and param count.
    char ch_payload[len - 4];// = (char*) &payload[4];
    memcpy(ch_payload, &payload[4], len - 3);
    
    // Split the payload at the first '|'
    char *tok = strtok(ch_payload, "|");
    
    // Set the name of the procedure.
    rp.name = malloc(strlen(tok));
    strncpy(rp.name, tok, strlen(tok));
    
    // Allocate memory for the parameters and split the remaining payload at '|'
    // until it's it fully consumed. Store the parameters as strings in the designated struct field.
    int i = 0;
    
    rp.params = malloc(sizeof(char*));
    tok = strtok(NULL, "|");
    while (tok) {
        rp.params[i] = malloc(strlen(tok));
        strncpy(rp.params[i++], tok, strlen(tok));
        tok = strtok(NULL, "|");
    }
    return rp;
}

int rpc_send_rhizome (const sid_t sid, const char *rpc_name, uint8_t *payload) {
    // Create new empty manifest and a bundle result struct for keeping track of particular execution results.*/
    rhizome_manifest *m = NULL;
    struct rhizome_bundle_result result = INVALID_RHIZOME_BUNDLE_RESULT;
    
    // Open the Rhizome database, the serval instance dir, the keyring file and unlock the keyring.
    if (rhizome_opendb() == -1){
        WARN("Could not open rhizome database. Aborting.");
        return -1;
    }
    if (create_serval_instance_dir() == -1){
        WARN("Could not open serval instance directory. Aborting.");
        return -1;
    }
    if (!(keyring = keyring_open_instance(""))){
        WARN("Could not open keyring file. Aborting.");
        return -1;
    }
    keyring_enter_pin(keyring, "");
    
    // Initialize the manifest.
    if ((m = rhizome_new_manifest()) == NULL){
        WARN("Could not create a new manifest. Aborting.");
        return -1;
    }
    
    rhizome_manifest *mout = NULL;
    rhizome_manifest_set_service(m, "RPC");
    rhizome_manifest_set_name(m, rpc_name);
    rhizome_manifest_set_sender(m, &my_subscriber->sid);
    rhizome_manifest_set_recipient(m, &sid);
    
    char tmp_file_name[L_tmpnam];
    
    tmpnam(tmp_file_name);
    
    FILE *rpc_file = fopen(tmp_file_name, "w+");
    
    fwrite(payload, sizeof(payload), sizeof(payload), rpc_file);
    
    // Close the file.
    fclose(rpc_file);
    
    result = rhizome_manifest_add_file(0, m, &mout, NULL, NULL, &my_subscriber->sid, tmp_file_name, 0, NULL);
    
    
    if (result.status != RHIZOME_BUNDLE_STATUS_NEW) {
        WARN("RPC is not new. Aborting.");
        return -1;
    }
    
    // Make sure, m is the same as mout, since the final manifest was written to mout in rhizome_manifest_add_file.
    // We now can throw away mout.
    if (mout != m) {
        rhizome_manifest_free(m);
        m = mout;
    }
    mout = NULL;
    
    // Create a playload_status variable for the status of the payload, since the payload was not imported yet.
    enum rhizome_payload_status pstatus;
    
    // Check, if the payload is new in terms of this payload is not in the rhizome store, yet.
    pstatus = rhizome_stat_payload_file(m, tmp_file_name);
    // If the payload is new, ...
    if (pstatus == RHIZOME_PAYLOAD_STATUS_NEW) {
        // store the payload.
        pstatus = rhizome_store_payload_file(m, tmp_file_name);
    }
    
    // Reset the result struct.
    rhizome_bundle_result_free(&result);
    // Per default, consider the payload is not valid.
    int pstatus_valid = 0;
    // Check for errors. If any of the below cases matches, the payload is valid, even, if it is an error.
    // It is only not valid, if one of the below cases does not match. I can't imagine a case, where this can happen, but ok.
    switch (pstatus) {
        case RHIZOME_PAYLOAD_STATUS_EMPTY:
        case RHIZOME_PAYLOAD_STATUS_STORED:
        case RHIZOME_PAYLOAD_STATUS_NEW:
            pstatus_valid = 1;
            result.status = RHIZOME_BUNDLE_STATUS_NEW;
            break;
        case RHIZOME_PAYLOAD_STATUS_TOO_BIG:
        case RHIZOME_PAYLOAD_STATUS_EVICTED:
            pstatus_valid = 1;
            result.status = RHIZOME_BUNDLE_STATUS_NO_ROOM;
            INFO("Insufficient space to store payload");
            break;
        case RHIZOME_PAYLOAD_STATUS_ERROR:
            pstatus_valid = 1;
            result.status = RHIZOME_BUNDLE_STATUS_ERROR;
            break;
        case RHIZOME_PAYLOAD_STATUS_WRONG_SIZE:
        case RHIZOME_PAYLOAD_STATUS_WRONG_HASH:
            pstatus_valid = 1;
            result.status = RHIZOME_BUNDLE_STATUS_INCONSISTENT;
            break;
        case RHIZOME_PAYLOAD_STATUS_CRYPTO_FAIL:
            pstatus_valid = 1;
            result.status = RHIZOME_BUNDLE_STATUS_READONLY;
            break;
    }
    if (!pstatus_valid)
        FATALF("pstatus = %d", pstatus);
    
    // If the status indicates, that the bundle is new, check if the manifest is valid and not malformed.
    if (result.status == RHIZOME_BUNDLE_STATUS_NEW) {
        if (!rhizome_manifest_validate(m) || m->malformed)
            result.status = RHIZOME_BUNDLE_STATUS_INVALID;
        else {
            // If everything seems to be okay, we can finalize the manifest. At this point, the RPC is written do disk.
            rhizome_bundle_result_free(&result);
            result = rhizome_manifest_finalise(m, &mout, 0);
            if (mout && mout != m && !rhizome_manifest_validate(mout)) {
                WHYF("Stored manifest id=%s is invalid -- overwriting", alloca_tohex_rhizome_bid_t(mout->cryptoSignPublic));
                rhizome_bundle_result_free(&result);
                result = rhizome_bundle_result(RHIZOME_BUNDLE_STATUS_NEW);
            }
        }
    }
    
    // Now we can clean up and free everything.
    unlink(tmp_file_name);
    int return_code = result.status;
    rhizome_bundle_result_free(&result);
    rhizome_manifest_free(m);
    keyring_free(keyring);
    keyring = NULL;
    
    return return_code;
}

// Execute the procedure
int rpc_excecute (struct rpc_procedure rp, MSP_SOCKET sock) {
    DEBUGF(rpc, "Executing %s.", rp.name);
    FILE *pipe_fp;
    
    // Compile the rpc name and the path of the binary to one string.
    char bin[strlen(rp.name) + strlen(SYSCONFDIR) + 17];
    FORMF_RPC_BIN_PATH(bin, "%s", rp.name);
    
    // Since we use popen, which expects a string where the binary with all parameters delimited by spaces is stored,
    // we have to compile the bin with all parameters from the struct.
    char *flat_params = malloc(strlen(bin) + sizeof(rp.params) / sizeof(rp.params[0]) + rp.paramc - 1);
    // This is the first part, containing bin and the first parameter, since there has to be at least one parameter.
    sprintf(flat_params, "%s %s", bin, rp.params[0]);
    // Now, we don't know, how many parameters will follow, so we have to do the remaining parameters programmatically in a loop.
    int i;
    for (i = 1; i < rp.paramc; i++) {
        strcat(flat_params, " ");
        strncat(flat_params, rp.params[i], strlen(rp.params[i]));
    }
    
    // Open the pipe.
    if ((pipe_fp = popen(flat_params, "r")) == NULL) {
        WARN("Could not open the pipe. Aborting.");
        return -1;
    }
    
    // Payload. Two bytes for packet type, 126 bytes for the result and 1 byte for '\0' to make sure the result will be a zero terminated string.
    uint8_t payload[2 + 127 + 1];
    write_uint16(&payload[0], RPC_PKT_CALL_RESPONSE);
    
    // If the pip is open ...
    if (pipe_fp) {
        // ... read the result, store it in the payload ...
        fgets((char *)&payload[2], 127, pipe_fp);
        payload[129] = (unsigned char) "\0";
        // ... and close the pipe.
        int ret_code = pclose(pipe_fp);
        if (WEXITSTATUS(ret_code) != 0) {
            WARNF("Execution of \"%s\" went wrong. See errormessages above for more information. Status %i", flat_params, WEXITSTATUS(ret_code));
            return -1;
        }
        DEBUG(rpc, "Returned result from Binary.");
    } else {
        return -1;
    }
    
    // If the connection is still alive, send the result back.
    if (!msp_socket_is_null(sock) && msp_socket_is_data(sock) == 1) {
        DEBUG(rpc, "Sending result via MSP.");
        msp_send(sock, payload, sizeof(payload));
        return 0;
    } else {
        DEBUG(rpc, "Sending result via RHIZOME.");
        rpc_send_rhizome(rp.caller_sid, rp.name, payload);
        return 0;
    }
    
    return -1;
}

// Handler of the RPC server
size_t server_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    size_t ret = 0;
    
    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_SHUTDOWN_REMOTE | MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        WARN("Socket closed.");
        msp_stop(sock);
        ret = 1;
    }
    
    // If we receive something, handle it.
    if (payload && len) {
        // First make sure, we received a RPC call packet.
        if (read_uint16(&payload[0]) == RPC_PKT_CALL) {
            DEBUG(rpc, "Received RPC via MSP.");
            // Parse the payload to the rpc_procedure struct
            struct rpc_procedure rp = rpc_parse_call(payload, len);
            
            // Check, if we offer this procedure.
            if (rpc_check_offered(&rp) == 0) {
                DEBUG(rpc, "Offering desired RPC. Sending ACK.");
                // Compile and send ACK packet.
                uint8_t ack_payload[2];
                write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
                ret = msp_send(sock, ack_payload, sizeof(ack_payload));
                
                // Try to execute the procedure.
                if (rpc_excecute(rp, sock) == 0) {
                    DEBUG(rpc, "RPC execution was successful.");
                    ret = len;
                } else {
                    ret = len;
                    status = 2;
                }
            } else {
                DEBUG(rpc, "Not offering desired RPC. Aborting.");
                ret = len;
                status = 1;
            }
        }
    }
    return ret;
}

void stopHandler (int signum) {
    DEBUGF(rpc, "Caught signal with signum %i. Stopping RPC server.", signum);
    running = 1;
}

int rpc_listen_rhizome () {
    rhizome_bid_t manifest_ids[3];
    rhizome_filehash_t file_ids[3];
    
    // Open the Rhizome database, the serval instance dir, the keyring file and unlock the keyring.
    if (rhizome_opendb() == -1){
        WARN("Could not open rhizome database. Aborting.");
        return -1;
    }
    if (create_serval_instance_dir() == -1){
        WARN("Could not open serval instance directory. Aborting.");
        return -1;
    }
    if (!(keyring = keyring_open_instance(""))){
        WARN("Could not open keyring file. Aborting.");
        return -1;
    }
    keyring_enter_pin(keyring, "");
    
    // Define the cursor, which iterates through the database.
    struct rhizome_list_cursor cursor;
    // For some reason, we need to zero out the memory, where the pointer to the cursor points to.
    bzero(&cursor, sizeof(cursor));
    // We set the cursor service to RPC, to skip MeshMS and files.
    cursor.service = "RPC";
    // Now we open the cursor.
    if (rhizome_list_open(&cursor) == -1) {
        keyring_free(keyring);
        keyring = NULL;
        return -1;
    }
    
    // Define a rhizome_read struct, where the reading of the content happens.
    struct rhizome_read read_state;
    // Again, we have to zero it out.
    bzero(&read_state, sizeof(read_state));
    
    // Iterate through the rhizome store.
    while (rhizome_list_next(&cursor) == 1) {
        // Get the current manifest.
        rhizome_manifest *m = cursor.manifest;
        
        // Open the file and prepare to read it.
        enum rhizome_payload_status status = rhizome_open_decrypt_read(m, &read_state);
        // If the file with the given hash is in the rhizome store, we read is.
        if (status == RHIZOME_PAYLOAD_STATUS_STORED) {
            // Create a buffer with the size if the file, where the content get written to.
            unsigned char buffer[m->filesize];
            // Read the content of the file, while its not empty.
            while((rhizome_read(&read_state, buffer, sizeof(buffer))) > 0){
                if (read_uint16(&buffer[0]) == RPC_PKT_CALL) {
                    DEBUG(rpc, "Received RPC via Rhizome.");
                    // Parse the payload to the rpc_procedure struct
                    struct rpc_procedure rp = rpc_parse_call(buffer, m->filesize);
                    rp.caller_sid = m->sender;
                    
                    // Check, if we offer this procedure.
                    if (rpc_check_offered(&rp) == 0) {
                        DEBUG(rpc, "Offering desired RPC. Sending ACK via Rhizome.");
                        // Compile and send ACK packet.
                        
                        uint8_t payload[2];
                        write_uint16(&payload[0], RPC_PKT_CALL_ACK);
                        
                        rpc_send_rhizome(rp.caller_sid, rp.name, payload);
                        
                        // Try to execute the procedure.
                        if (rpc_excecute(rp, MSP_SOCKET_NULL) == 0) {
                            DEBUG(rpc, "RPC execution was successful.");
                        } else {
                            status = 2;
                        }
                    } else {
                        DEBUG(rpc, "Not offering desired RPC. Aborting.");
                        status = 1;
                    }
                }
            }
        }
    }
    rhizome_list_release(&cursor);
    keyring_free(keyring);
    return 0;
}

DEFINE_CMD(rpc_recv_cli, 0,
           "Just a Test",
           "listen");
int rpc_recv_cli(const struct cli_parsed *UNUSED(parsed), struct cli_context *UNUSED(context)){
    return rpc_listen();
}

// Main RPC server listener.
int rpc_listen () {
    // Create address struct ...
    struct mdp_sockaddr addr;
    bzero(&addr, sizeof addr);
    // ... and set the sid to a local sid and port where to listen at.
    addr.port = 112;
    addr.sid = BIND_PRIMARY;
    
    // Create MDP socket.
    mdp_fd = mdp_socket();
    
    // Sockets should not block.
    set_nonblock(mdp_fd);
    set_nonblock(STDIN_FILENO);
    set_nonblock(STDOUT_FILENO);
    
    // Create MSP socket.
    MSP_SOCKET sock = msp_socket(mdp_fd, 0);
    // Connect to local socker ...
    msp_set_local(sock, &addr);
    // ... and listen it.
    msp_listen(sock);
    
    // Set the handler to handle incoming packets.
    msp_set_handler(sock, server_handler, NULL);
    
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    
    // Listen.
    while(running == 0){
        rpc_listen_rhizome();
        // Process MSP socket
        time_ms_t next_time;
        msp_processing(&next_time);
        
        msp_recv(mdp_fd);
        
        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);
        
    }
    
    sock = MSP_SOCKET_NULL;
    msp_close_all(mdp_fd);
    mdp_close(mdp_fd);
    
    return 0;
}