#include "rpc.h"

char *current_rpc = "";
char *current_sid = "";

// Function to flatten the parameters and replace the first parameter if it is a local path.
int _rpc_client_msp_replace_if_path (char *flat_params, char **params, int paramc) {
    if (!access(params[0], F_OK)) {

        char *new_params[paramc];
        new_params[0] = "file";

        int i;
        for (i = 1; i < paramc; i++) {
            new_params[i] = (char *) params[i];
        }

        char *flat = _rpc_flatten_params(paramc, (char **) new_params, "|");
        strcpy(flat_params, flat);
        free(flat);
    } else {
        char *flat = _rpc_flatten_params(paramc, params, "|");
        strcpy(flat_params, flat);
        free(flat);
    }
    return 0;
}

// The RPC client handler
size_t _rpc_client_msp_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
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
			if (_rpc_str_is_filehash((char *) rpc_result)) {
				char fpath[128 + strlen(current_rpc) + 3];
				while (_rpc_download_file(fpath, current_rpc, current_sid) != 0) sleep(1);
				memcpy(rpc_result, fpath, 128 + strlen(current_rpc) + 3);
			}
            received = 2;
        }
    }
    return ret;
}

// Direct call function.
int rpc_client_call_msp (sid_t sid, char *rpc_name, int paramc, char **params) {
	// Check if the sid is even reachable before doing anything else.
	if (!_rpc_sid_is_reachable(sid)) {
		return -1;
	}
    // Make sure the result array is empty.
    memset(rpc_result, 0, sizeof(rpc_result));
	current_sid = alloca_tohex_sid_t(sid);
	current_rpc = rpc_name;

    // Create address struct ...
    struct mdp_sockaddr addr;
    bzero(&addr, sizeof addr);
    // ... and set the sid and port of the server.
    addr.sid = sid;
    addr.port = MDP_PORT_RPC_MSP;

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
    msp_set_handler(sock, _rpc_client_msp_handler, NULL);

	// Flatten the params and replace the first parameter if it is a local path.
	char flat_params[512];
    _rpc_client_msp_replace_if_path(flat_params, params, paramc);

    // Check if the first parameter after the first | is "file".
    // If it is, send it over MSP. Otherwise do the normal stuff.
    if (!strncmp(&flat_params[1], "file", 4)) {
        FILE *payload_file = fopen(params[0], "r");
        // Get the size.
        fseek(payload_file, 0L, SEEK_END);
        size_t file_size = ftell(payload_file);
        rewind(payload_file);

        // Headersize: 2 byte packet type, 2 byte #parameters, rpc_name strlen bytes, params strlen bytes
        // 3 bytes "::\0" seperator, 8 bytes filesize, 8 bytes already sent (including this packet), 8 bytes timestamp
        size_t base_header_size = 2 + 2 + strlen(rpc_name) + strlen(flat_params);
        size_t header_size = base_header_size + 3 + 8 + 8 + 8;
        size_t payload_size = 1024 - header_size;
        time_t call_time = time(NULL);

        char buffer[payload_size];
        size_t i;
        size_t remaining_size = file_size;
        time_ms_t next_time;
        for (i = 0; i < file_size; i += payload_size, remaining_size -= payload_size) {
            // If there is less than payload_size bytes left from the file, we only send
            // the remaining data. payload_size bytes otherwise.
            size_t bytes_to_send = remaining_size < payload_size ? remaining_size : payload_size;

            // Read chunk from file.
            size_t UNUSED(read_size) = fread(buffer, 1, bytes_to_send, payload_file);

            // Create payload.
            uint8_t payload[header_size + payload_size];
            // Fill default values.
            _rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params);

            // Write "::\0" seperator.
            memcpy(&payload[base_header_size], (uint8_t *)"::\0", 3);
            // Write filesize to payload.
            write_uint64(&payload[base_header_size + 3], file_size);
            // Write already sent size
            write_uint64(&payload[base_header_size + 3 + 8], i + bytes_to_send);
            // Write timestamp
            write_uint64(&payload[base_header_size + 3 + 8 + 8], call_time);
            // Copy the buffer to payload.
            memcpy(&payload[base_header_size + 3 + 8 + 8 + 8], (uint8_t *)buffer, bytes_to_send);

            // TODO: What, if sock is closed?
            // Send.
            msp_send(sock, payload, sizeof(payload));

            msp_processing(&next_time);

            sleep(1);
        }

        fclose(payload_file);

    } else {
        uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
        _rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params);

        // Send the payload.
        msp_send(sock, payload, sizeof(payload));
    }




	// While we have not received the answer...
    while (received == 0 || received == 1) {
		// If the socket is closed, start the Rhizome listener, but only if this was a transparetn call. Otherwise we just return.
		// No reachablility check required since the server was reachabel once. This check below is sufficient.
		if (msp_socket_is_null(sock) && !msp_socket_is_data(sock)) {
			pfatal("MSP socket closed. Aborting.");
            // Clean up.
            sock = MSP_SOCKET_NULL;
            msp_close_all(mdp_fd);
            mdp_close(mdp_fd);
            return -1;
		}

        // Process MSP socket
        time_ms_t next_time;
        msp_processing(&next_time);
        time_ms_t poll_timeout = next_time - gettime_ms();

        // Poll the socket
        poll(fds, 1, poll_timeout);

        // If something arrived, receive it.
        if (fds->revents & POLLIN) {
            msp_recv(mdp_fd);
        }
    }

    // Clean up.
    sock = MSP_SOCKET_NULL;
    msp_close_all(mdp_fd);
    mdp_close(mdp_fd);

    return received;
}

