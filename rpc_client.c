#include "rpc.h"

int received = 0;

// NOT NEEDED SO FAR! WILL BE NEEDED FOR BROADCASTS.
//int rpc_discover (sid_t server_sid, char *rpc_name) {
//    int mdp_sockfd;
//    if ((mdp_sockfd = mdp_socket()) < 0) {
//        return WHY("Cannot create MDP socket");
//    }
//
//    struct mdp_header mdp_header;
//    bzero(&mdp_header, sizeof(mdp_header));
//
//    mdp_header.local.sid = BIND_PRIMARY;
//    mdp_header.remote.sid = server_sid;
//    mdp_header.remote.port = MDP_PORT_RPC_DISCOVER;
//    mdp_header.qos = OQ_MESH_MANAGEMENT;
//    mdp_header.ttl = PAYLOAD_TTL_DEFAULT;
//
//    if (mdp_bind(mdp_sockfd, &mdp_header.local) < 0){
//        printf("RPC WARN: COULD NOT BIND\n");
//        return -1;
//    }
//
//    time_ms_t finish = gettime_ms() + 10;
//    int acked = 0;
//
//    while (gettime_ms() < finish && acked != 1) {
//        uint8_t payload[strlen(rpc_name) + 2];
//        write_uint16(&payload[0], RPC_PKT_DISCOVER);
//        memcpy(&payload[2], rpc_name, strlen(rpc_name));
//
//        if (mdp_send(mdp_sockfd, &mdp_header, payload, sizeof(payload)) < 0){
//            printf("RPC DEBUG: SEND FAIL\n");
//            return -1;
//        }
//        if (mdp_poll(mdp_sockfd, 500)<=0)
//            continue;
//
//        struct mdp_header mdp_recv_header;
//        uint8_t recv_payload[strlen(rpc_name)];
//        ssize_t len = mdp_recv(mdp_sockfd, &mdp_recv_header, recv_payload, sizeof(recv_payload));
//
//        if (len < 0) {
//            printf("RPC DEBUG: LEN FAIL\n");
//            break;
//        }
//
//        if (read_uint16(&recv_payload[0]) == RPC_PKT_DISCOVER_ACK) {
//            acked = 1;
//            printf("RPC DEBUG: RECEIVED: %d, LEN: %i\n", read_uint16(&recv_payload[0]), len);
//        }
//
//
//    }
//    return acked;
//}

// The RPC cliend handler
size_t client_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    size_t ret = 0;

    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        printf("RPC WARN: Socket closed.\n");
        received = received == 1 || received == 2 ? received : -1;
        msp_stop(sock);
    }

    // If the other site closed the connection, we do also.
    if (state & MSP_STATE_SHUTDOWN_REMOTE) {
        printf("RPC WARN: Socket shutdown\n");
        received = received == 1 || received == 2 ? received : -1;
        msp_shutdown(sock);
    }

    // If we have payload handle it.
    if (payload && len) {
        ret = len;
        // Get the packet type.
        uint16_t pkt_type = read_uint16(&payload[0]);
        // If we receive an ACK, just print.
        if (pkt_type == RPC_PKT_CALL_ACK) {
            printf("RPC DEBUG: Server accepted call.\n");
            received = 1;
        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
            printf("RPC DEBUG: Answer received.\n");
            memcpy(rpc_result, &payload[2], len - 2);
            received = 2;
        }
    }
    return ret;
}

// Direct call function.
int rpc_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    // Create address struct ...
    struct mdp_sockaddr addr;
    bzero(&addr, sizeof addr);
    // ... and set the sid and port of the server.
    addr.sid = sid;
    addr.port = 112;

    // Create MDP socket.
    int mdp_fd = mdp_socket();

    // Sockets should not block.
    set_nonblock(mdp_fd);
    set_nonblock(STDIN_FILENO);
    set_nonblock(STDOUT_FILENO);

    // Create a poll struct, where the polled packets are handled.
    struct pollfd fds[2];
    fds->fd = mdp_fd;
    fds->events = POLLIN | POLLERR;

    // Create MSP socket.
    MSP_SOCKET sock = msp_socket(mdp_fd, 0);
    // Connect to the server.
    msp_connect(sock, &addr);

    // Set the handler to handle incoming packets.
    msp_set_handler(sock, client_handler, NULL);

    // Prepare 2D array of params to 1D for serialization. Use '|' as a seperator.
    char *flat_params  = malloc(sizeof(char *));
    sprintf(flat_params, "|%s", params[0]);
    int i;
    for (i = 1; i < paramc; i++) {
        strcat(flat_params, "|");
        strncat(flat_params, params[i], strlen(params[i]));
    }

    // Create the payload containing ...
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params)];
    // ... the packet type, ...
    write_uint16(&payload[0], RPC_PKT_CALL);
    // ... number of parameters, ...
    write_uint16(&payload[2], (uint16_t) paramc);
    // ... the rpc name ...
    memcpy(&payload[4], rpc_name, strlen(rpc_name));
    // ... and all params.
    memcpy(&payload[4 + strlen(rpc_name)], flat_params, strlen(flat_params));

    printf("RPC DEBUG: Calling %s for %s.\n", alloca_tohex_sid_t(sid), rpc_name);

    // Send the payload.
    msp_send(sock, payload, sizeof(payload));

    time_t timeout = time(NULL);

    // While we have not received the answer...
    while(received == 0 || received == 1){
        // Process MSP socket
        time_ms_t next_time;
        msp_processing(&next_time);
        time_ms_t poll_timeout = next_time - gettime_ms();

        // We only wait for 3 seconds.
        if ((double) (time(NULL) - timeout) >= 3.0) {
            break;
        }

        // Poll the socket
        poll(fds, 1, poll_timeout);

        // If something arrived, receive it.
        if (fds->revents & POLLIN){
            msp_recv(mdp_fd);
        }
    }

    // Clean up.
    sock = MSP_SOCKET_NULL;
    msp_close_all(mdp_fd);
    mdp_close(mdp_fd);

    return received;
}

void rpc_write_file (const char *rpc_name, const int paramc, const char **params, char *filepath) {
    // Open the file.
    FILE *rpc_file = fopen(filepath, "w+");

    // Prepare 2D array of params to 1D for serialization. Use '|' as a seperator.
    char *flat_params  = malloc(sizeof(char *));
    sprintf(flat_params, "|%s", params[0]);
    int i;
    for (i = 1; i < paramc; i++) {
        strcat(flat_params, "|");
        strncat(flat_params, params[i], strlen(params[i]));
    }

    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params)];
    write_uint16(&payload[0], RPC_PKT_CALL);
    write_uint16(&payload[2], (uint16_t) paramc);
    memcpy(&payload[4], rpc_name, strlen(rpc_name));
    memcpy(&payload[4 + strlen(rpc_name)], flat_params, strlen(flat_params) + 1);

    fwrite(payload, sizeof(payload), sizeof(payload), rpc_file);

    // Close the file.
    fclose(rpc_file);
}

int rpc_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    int return_code = -1;

    rhizome_bid_t manifest_ids[3];
    rhizome_filehash_t file_ids[3];

    // Create new empty manifest and a bundle result struct for keeping track of particular execution results.*/
    rhizome_manifest *m = NULL;
    struct rhizome_bundle_result result = INVALID_RHIZOME_BUNDLE_RESULT;

    // Open the Rhizome database, the serval instance dir, the keyring file and unlock the keyring.
    if (rhizome_opendb() == -1){
        printf("RPC WARN: Could not open rhizome database. Aborting.\n");
        return -1;
    }
    if (create_serval_instance_dir() == -1){
        printf("RPC WARN: Could not open serval instance directory. Aborting.\n");
        return -1;
    }
    if (!(keyring = keyring_open_instance(""))){
        printf("RPC WARN: Could not open keyring file. Aborting.\n");
        return -1;
    }
    keyring_enter_pin(keyring, "");

    // Initialize the manifest.
    if ((m = rhizome_new_manifest()) == NULL){
        printf("RPC WARN: Could not create a new manifest. Aborting.\n");
        return -1;
    }

    rhizome_manifest *mout = NULL;
    rhizome_manifest_set_service(m, "RPC");
    rhizome_manifest_set_name(m, rpc_name);
    rhizome_manifest_set_sender(m, &my_subscriber->sid);
    rhizome_manifest_set_recipient(m, &sid);

    char tmp_file_name[L_tmpnam];

    tmpnam(tmp_file_name);

    printf("RPC DEBUG: Filename: %s", tmp_file_name);

    rpc_write_file(rpc_name, paramc, params, tmp_file_name);

    result = rhizome_manifest_add_file(0, m, &mout, NULL, NULL, &my_subscriber->sid, tmp_file_name, 0, NULL);


    if (result.status != RHIZOME_BUNDLE_STATUS_NEW) {
        printf("RPC WARN: RPC is not new. Aborting.\n");
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
            printf("RPC INFO: Insufficient space to store payload.\n");
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

    manifest_ids[0] = m->cryptoSignPublic;
    file_ids[0] = m->filehash;

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

    time_t timeout = time(NULL);
    while (received == 0) {
        // We only wait for 3 seconds.
        if ((double) (time(NULL) - timeout) >= 3.0) {
            break;
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
                    if (read_uint16(&buffer[0]) == RPC_PKT_CALL_ACK){
                        printf("RPC DEBUG: Received ACK via Rhizome. Waiting.\n");
                        manifest_ids[1] = m->cryptoSignPublic;
                        file_ids[1] = m->filehash;
                    } else if (read_uint16(&buffer[0]) == RPC_PKT_CALL_RESPONSE) {
                        printf("RPC DEBUG: Received result.\n");
                        memcpy(rpc_result, &buffer[2], m->filesize - 2);
                        return_code = 2;
                        received = 1;
                        manifest_ids[2] = m->cryptoSignPublic;
                        file_ids[2] = m->filehash;
                        break;
                    }
                }
                if (return_code == 2) {
                    break;
                }
            }
        }
        sleep(1);
    }

    // Now we can clean up and free everything.
    remove(tmp_file_name);
    rhizome_list_release(&cursor);
    rhizome_bundle_result_free(&result);
    int i;
    for (i = 0; i < 3; i++) {
        rhizome_delete_file(&file_ids[i]);
        rhizome_delete_manifest(&manifest_ids[i]);
    }
    rhizome_manifest_free(m);
    keyring_free(keyring);
    keyring = NULL;

    return return_code;
}

// General call function. For transparent usage.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params) {
    // TODO: THIS IS FOR ANY/ALL RPCS.
    if (is_sid_t_any(server_sid) || is_sid_t_broadcast(server_sid)) {
        return -1;
    } else {
        // Call the rpc directly over msp.
        int call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);//rpc_call_msp(server_sid, rpc_name, paramc, params);

        if (call_return == -1) {
            printf("RPC WARN: Server not available via MSP. Trying Rhizome.\n");
            //call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);
            return -1;
        } else if (call_return == 0) {
            printf("RPC DEBUG: NOT RECEIVED ACK\n");
            return -1;
        } else if (call_return == 1) {
            printf("RPC DEBUG: COULD NOT COLLECT RESULT AFTER ACK\n");
            return -1;
        } else {
            return 0;
        }
    }

    return 1;
}
