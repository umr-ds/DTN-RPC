#include "rpc.h"

char *current_rpc = "";
char *current_sid = "";

// The RPC client handler
size_t _rpc_client_msp_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
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
        // Get the packet type.
        uint8_t pkt_type = read_uint8(&payload[0]);
        // If we receive an ACK, just print.
        if (pkt_type == RPC_PKT_CALL_ACK) {
            pinfo("Server accepted call.");
            received = 1;
        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
            pinfo("Answer received.");
            // We need the server SID which we have to get programmatically.
            struct mdp_sockaddr addr;
            bzero(&addr, sizeof addr);
            msp_get_remote(sock, &addr);
			if (_rpc_str_is_filehash((char *) &payload[1])) {
				char fpath[128 + strlen(current_rpc) + 3];
				while (_rpc_download_file(fpath, current_rpc, current_sid) != 1) sleep(1);
				memcpy(rpc_result[0].content, fpath, 128 + strlen(current_rpc) + 3);
			} else {
                memcpy(&rpc_result[0].content, &payload[1], len - 1);
            }
            memcpy(&rpc_result[0].server_sid, &addr.sid, sizeof(sid_t));
            received = 2;
        }
    }
    return len;
}

// Direct call function.
int rpc_client_call_msp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements) {
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
	_rpc_client_replace_if_path(flat_params, rpc_name, params, paramc);

    // Compile the call payload.
    uint8_t payload[1 + 4 + 1 + strlen(rpc_name) + strlen(flat_params) + 1];
	_rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params, requirements);

	// Send the payload.
	msp_send(sock, payload, sizeof(payload));


	// While we have not received the answer...
    while (received == 0 || received == 1) {
		// If the socket is closed, start the Rhizome listener, but only if this was a transparetn call. Otherwise we just return.
		// No reachablility check required since the server was reachabel once. This check below is sufficient.
		if (msp_socket_is_null(sock) && !msp_socket_is_data(sock)) {
            pwarn("MSP socket closed. Starting Rhizome listener.");
            // Clean up.
            sock = MSP_SOCKET_NULL;
            msp_close_all(mdp_fd);
            mdp_close(mdp_fd);
            return _rpc_client_rhizome_listen();
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

