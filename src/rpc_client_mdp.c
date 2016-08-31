#include "rpc.h"

// MDP call function.
int rpc_client_call_mdp (sid_t server_sid, char *rpc_name, int paramc, char **params) {
	// If the server_sid is not broadcast, we have to check, if the server is available.
	if (!is_sid_t_broadcast(server_sid) && !_rpc_sid_is_reachable(server_sid)) {
		return -1;
	}

	// Make sure the result array is empty.
	memset(rpc_result, 0, sizeof(rpc_result));
	// Open the mdp socket.
	int mdp_sockfd;
	if ((mdp_sockfd = mdp_socket()) < 0) {
		return WHY("Cannot create MDP socket");
	}

	// Set my own local SID, the broadcast SID for remote and the MDP port for RPCs.
	struct mdp_header mdp_header;
	bzero(&mdp_header, sizeof(mdp_header));
	mdp_header.local.sid = BIND_PRIMARY;
	mdp_header.remote.sid = server_sid;
	mdp_header.remote.port = MDP_PORT_RPC;
	if (is_sid_t_broadcast(server_sid)) {
	    mdp_header.flags = MDP_FLAG_NO_CRYPT;
	}

	// Bind the my SID to the socket.
	if (mdp_bind(mdp_sockfd, &mdp_header.local) < 0) {
		pfatal("Could not bind to broadcast address.");
		return -1;
	}

	// Flatten the params and replace the first parameter if it is a local path.
	char flat_params[512];
	_rpc_client_replace_if_path(flat_params, rpc_name, params, paramc);

	// Compile the call payload.
	uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params);

	// Send the payload.
	if (mdp_send(mdp_sockfd, &mdp_header, payload, sizeof(payload)) < 0) {
		pfatal("Could not discover packet. Aborting.");
		return -1;
	}

	// pollfd for listening for the result via MDP.
	struct pollfd fds[2];
    fds->fd = mdp_sockfd;
    fds->events = POLLIN;

	time_t start_time = time(NULL);
	int wait_time = 10;
	while (received == 0 || received == 1) {

		if (time(NULL) - start_time > wait_time) {
		    mdp_close(mdp_sockfd);
			return -1;
		}
		// Poll the socket
        poll(fds, 1, 500);

        // If something arrived, receive it.
        if (fds->revents & POLLIN) {
			struct mdp_header mdp_recv_header;

			// Set the payloadsize.
			uint8_t recv_payload[MDP_MTU];
			ssize_t incoming_len = mdp_recv(mdp_sockfd, &mdp_recv_header, recv_payload, sizeof(recv_payload));

			if (incoming_len < 0) {
				pwarn("Received empty packet. Ignoring.");
				continue;
			}

			// Get the packet type.
	        uint16_t pkt_type = read_uint16(&recv_payload[0]);
	        // If we receive an ACK, just print.
	        if (pkt_type == RPC_PKT_CALL_ACK) {
	            pinfo("Server accepted call.");
	            received = 1;
	        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
				// If we received the result, copy it to the result array.
	            pinfo("Answer received.");
	            memcpy(rpc_result, &recv_payload[2], incoming_len - 2);
				if (_rpc_str_is_filehash((char *) rpc_result)) {
					char fpath[128 + strlen(rpc_name) + 3];
					while (_rpc_download_file(fpath, rpc_name, alloca_tohex_sid_t(SID_BROADCAST)) != 0) sleep(1);
					memcpy(rpc_result, fpath, 128 + strlen(rpc_name) + 3);
				}
	            received = 2;
	        }
		}
	}

	// Cleanup.
    mdp_close(mdp_sockfd);
	_rpc_rhizome_invalidate();

	return received;
}
