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
//        pfatal("COULD NOT BIND");
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
//            pfatal("SEND FAIL");
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
//            printf(RPC DEBUG "LEN FAIL\n" RPC_RESET);
//            break;
//        }
//
//        if (read_uint16(&recv_payload[0]) == RPC_PKT_DISCOVER_ACK) {
//            acked = 1;
//            printf(RPC DEBUG "RECEIVED: %d, LEN: %i\n" RPC_RESET, read_uint16(&recv_payload[0]), len);
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
        pwarn("Socket closed.");
        received = received == 1 || received == 2 ? received : -1;
        msp_stop(sock);
    }

    // If the other site closed the connection, we do also.
    if (state & MSP_STATE_SHUTDOWN_REMOTE) {
        pwarn("Socket shutdown.");
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
            pinfo("Server accepted call.");
            received = 1;
        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
            pinfo("Answer received.");
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

    char *flat_params = _rpc_flatten_params(paramc, params, "|");

    // Construct the payload and write it to the payload file.
    // |------------------------|-------------------|----------------------------|--------------------------|
    // |-- 2 byte packet type --|-- 2 byte paramc --|-- strlen(rpc_name) bytes --|-- strlen(params) bytes --|
    // |------------------------|-------------------|----------------------------|--------------------------|
    // 1 extra byte for string termination.
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, flat_params);

    pinfo("Calling %s for %s.", alloca_tohex_sid_t(sid), rpc_name);

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

int rpc_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    int return_code = -1;

    // Flatten the params.
    char *payload_flat_params = _rpc_flatten_params(paramc, params, "|");

    // Construct the payload and write it to the payload file.
    // |------------------------|-------------------|----------------------------|--------------------------|
    // |-- 2 byte packet type --|-- 2 byte paramc --|-- strlen(rpc_name) bytes --|-- strlen(params) bytes --|
    // |------------------------|-------------------|----------------------------|--------------------------|
    // One extra byte for string termination.
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(payload_flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, payload_flat_params);
    char tmp_payload_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_payload_file_name, payload, sizeof(payload));

    // Construct the manifest and write it to the manifest file.
    int manifest_size = strlen("service=RPC\nname=\nsender=\nrecipient=\n") + strlen(rpc_name) + (strlen(alloca_tohex_sid_t(sid)) * 2);
    char manifest_str[manifest_size];
    sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));
    char tmp_manifest_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));

    // Init the cURL stuff.
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        return_code = -1;
        goto clean_rhizome_client_call;
    }

    // Declare all needed headers forms and URLs.
    struct curl_slist *header = NULL;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    char *url_insert = "http://localhost:4110/restful/rhizome/insert";

    // Set basic cURL options (see function).
    _curl_set_basic_opt(url_insert, curl_handler, header);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);


    // Add the manifest and payload form and add the form to the cURL request.
    _curl_add_file_form(tmp_manifest_file_name, tmp_payload_file_name, curl_handler, formpost, lastptr);

    // Perfom request, which means insert the RPC file to the store.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (post): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_client_call_all;
    }

    /* LISTENER PART (waiting for response) */
    // Reinit to forget about the post part above.
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in get. Aborting.");
        return_code = -1;
        goto clean_rhizome_client_call_all;
    }

    // We only wait for a few seconds (while developing. Lateron definitely longer).
    time_t timeout = time(NULL);
    int waittime = 10;
    while (received == 0) {
        if ((double) (time(NULL) - timeout) >= waittime) {
            break;
        }

        // Remove everything from the cURL memory.
        _curl_reinit_memory(&curl_result_memory);

        header = NULL;
        char *url_get = "http://localhost:4110/restful/rhizome/bundlelist.json";

        // Again, set basic options, ...
        _curl_set_basic_opt(url_get, curl_handler, header);
        // ... but this time add a callback function, where results from the cURL call are handled.
        curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _curl_write_response);
        curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

        // Get the bundlelist.
        curl_res = curl_easy_perform(curl_handler);
        if (curl_res != CURLE_OK) {
            pfatal("CURL failed (get): %s. Aborting.", curl_easy_strerror(curl_res));
            return_code = -1;
            goto clean_rhizome_client_call_all;
        }

        // Write the bundlelist to a string for manipulations.
        char json_string[(size_t)curl_result_memory.size - 1];
        memcpy(json_string, curl_result_memory.memory, (size_t)curl_result_memory.size - 1);

        // Parse JSON:
        // Init, ...
        cJSON *incoming_json = cJSON_Parse(json_string);
        // ... get the 'rows' entry, ...
        cJSON *rows = cJSON_GetObjectItem(incoming_json, "rows");
        // ... consider only the recent file, ...
        cJSON *recent_file = cJSON_GetArrayItem(rows, 0);
        // ... get the 'service', ...
        char *service = cJSON_GetArrayItem(recent_file, 2)->valuestring;
        // ... the recipient from the recent file.
        char *recipient = cJSON_GetArrayItem(recent_file, 12)->valuestring;

        // Check, if this file is an RPC packet and if it is not from but for the client.
        int service_is_rpc = strncmp(service, "RPC", strlen("RPC")) == 0;
        int not_my_file = recipient != NULL && strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid)) == 0;

        if (service_is_rpc  && not_my_file) {
            // Free everyhing, again.
            _curl_reinit_memory(&curl_result_memory);
            curl_slist_free_all(header);
            header = NULL;

            // Get the bundle ID of the file which should be decrypted.
            char *bid = cJSON_GetArrayItem(recent_file, 3)->valuestring;
            char url_decrypt[117];
            sprintf(url_decrypt, "http://localhost:4110/restful/rhizome/%s/decrypted.bin", bid);

            _curl_set_basic_opt(url_decrypt, curl_handler, header);

            // Decrypt the file.
            curl_res = curl_easy_perform(curl_handler);
            if (curl_res != CURLE_OK) {
                pfatal("CURL failed (decrypt): %s.", curl_easy_strerror(curl_res));
                return_code = -1;
                goto clean_rhizome_client_call_all;
            }

            // Copy the payload for manipulations.
            size_t filesize = cJSON_GetArrayItem(recent_file, 9)->valueint;
            uint8_t recv_payload[filesize];
            memcpy(recv_payload, curl_result_memory.memory, filesize);

            if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL_ACK){
                // If we got an ACK packet, we wait (for now) 5 more seconds.
                pinfo("Received ACK via Rhizome. Waiting.");
                waittime = 20;
            } else if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL_RESPONSE) {
                // We got our result!
                pinfo("Received result.");
                memcpy(rpc_result, &recv_payload[2], filesize - 2);
                return_code = 2;
                received = 1;
                break;
            }
        }
        sleep(1);
    }

    // Clean up.
    clean_rhizome_client_call_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_client_call:
        _curl_free_memory(&curl_result_memory);
        remove(tmp_manifest_file_name);
        remove(tmp_payload_file_name);

        return return_code;
}

// General call function. For transparent usage.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params) {
    // TODO: THIS IS FOR ANY/ALL RPCS.
    if (is_sid_t_any(server_sid) || is_sid_t_broadcast(server_sid)) {
        return -1;
    } else {
        // Call the rpc directly over msp.
        int call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);
        // int call_return = rpc_call_msp(server_sid, rpc_name, paramc, params);

        if (call_return == -1) {
            pwarn("Server not available via MSP. Trying Rhizome.");
            //call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);
            return -1;
        } else if (call_return == 0) {
            pdebug("NOT RECEIVED ACK");
            return -1;
        } else if (call_return == 1) {
            pdebug("COULD NOT COLLECT RESULT AFTER ACK");
            return -1;
        } else {
            return 0;
        }
    }

    return 1;
}
