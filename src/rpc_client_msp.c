#include "rpc.h"

char *current_rpc = "";
char *current_sid = "";

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

// Function to flatten the parameters and replace the first parameter if it is a local path.
// The difference here is that the file will not be stored in the Rhizome store. Instead,
// it get' sent via MSP.
int _rpc_client_msp_replace_if_path (char *flat_params, char **params, int paramc) {
	if (!access(params[0], F_OK)) {
		FILE *fp = fopen(params[0], "r");
		fseek(fp, 0L, SEEK_END);
		size_t file_len = ftell(fp);
		rewind(fp);
		fclose(fp);
		char file_size[256];
		sprintf(file_size, "size:%lu", file_len);

		char *new_params[paramc];
		new_params[0] = file_size;

		int i;
		for (i = 1; i < paramc; i++) {
			new_params[i] = (char *) params[i];
		}

		char *flat = _rpc_flatten_params(paramc, (char **) new_params, "|");
		strcpy(flat_params, flat);
		free(flat);
		return 1;
	} else {
		char *flat = _rpc_flatten_params(paramc, params, "|");
		strcpy(flat_params, flat);
		free(flat);
		return 2;
	}
	return 0;
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
	int first_param_type = _rpc_client_msp_replace_if_path(flat_params, params, paramc);
	size_t base_payload_size = 2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1;

	// Send the call packet.
	uint8_t payload[base_payload_size];
	_rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params);
	msp_send(sock, payload, sizeof(payload));

	// If we identified the first parameter as a file, we have to send it via MSP.
	if (first_param_type == 1) {
		// Open the file.
		FILE *fp = fopen(params[0], "r");
		// Get the size.
		fseek(fp, 0L, SEEK_END);
		size_t file_len = ftell(fp);
		rewind(fp);
		// Read the file to a buffer.
		char file_buffer[file_len];
		size_t UNUSED(read_size) = fread(file_buffer, file_len, 1, fp);
		// Close the file.
		fclose(fp);

		// Get current time for identifying the file at server side.
		time_t send_time = time(NULL);
		uint8_t *data = NULL;
		size_t i;
		size_t remaining_size = file_len;
		for (i = 0; i < file_len; i+=1024, remaining_size-=1024) {
			// If there is less than 1024 bytes left from the file, we only send
			// the remaining data. 1024 bytes otherwise.
			size_t bytes_to_send = remaining_size < 1024 ? remaining_size : 1024;
			data = realloc(data, 10 + bytes_to_send);
			memset(data, 0, 10 + bytes_to_send);
			write_uint16(&data[0], RPC_PKT_CALL_CHUNK);
			write_uint64(&data[2], send_time);
			memcpy(&data[10], (uint8_t *)&file_buffer[i], bytes_to_send);
			msp_send(sock, data, 10 + bytes_to_send);
		}
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
	_rpc_rhizome_invalidate();

    return received;
}

