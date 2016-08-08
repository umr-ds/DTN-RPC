#include "rpc.h"

// MDP call function. For broadcasts.
int rpc_client_call_mdp_broadcast (const char *rpc_name, const int paramc, const char **params) {
	// Open the mdp socket.
	int mdp_sockfd;
	if ((mdp_sockfd = mdp_socket()) < 0) {
		return WHY("Cannot create MDP socket");
	}

	// Set my own local SID, the broadcast SID for remote and the MDP port for RPCs.
	struct mdp_header mdp_header;
	bzero(&mdp_header, sizeof(mdp_header));
	mdp_header.local.sid = BIND_PRIMARY;
	mdp_header.remote.sid = BIND_ALL;
	mdp_header.remote.port = MDP_PORT_RPC;

	// Bind the my SID to the socket.
	if (mdp_bind(mdp_sockfd, &mdp_header.local) < 0) {
		pfatal("Could not bind to broadcast address.");
		return -1;
	}

	// Flatten the params for serialization.
	char *flat_params;
	// If the first parameter is filehash, we have to handle them seperately.
	if (strncmp(params[0], "filehash", strlen("filehash")) == 0 && paramc >= 2) {
		// Add the file to the Rhizome store given as the second parameter and replace the local path with the filehash.
		char filehash[129];
		_rpc_add_file_to_store(filehash, SID_BROADCAST, rpc_name, params[1]);

		char *new_params[paramc];
		new_params[0] = (char *) params[0];
		new_params[1] = filehash;

		int i;
		for (i = 2; i < paramc; i++) {
			new_params[i] = (char *) params[i];
		}

		flat_params = _rpc_flatten_params(paramc, (const char **) new_params, "|");
	} else {
		flat_params = _rpc_flatten_params(paramc, params, "|");
	}

	// Compile the call payload.
	uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, flat_params);

	// Send the payload.
	if (mdp_send(mdp_sockfd, &mdp_header, payload, sizeof(payload)) < 0) {
		pfatal("Could not discover packet. Aborting.");
		return -1;
	}

	// pollfd for listening for the result via MDP.
	struct pollfd fds[2];
    fds->fd = mdp_sockfd;
    fds->events = POLLIN;

	while (received == 0 || received == 1) {
		// Poll the socket
        poll(fds, 1, 500);

        // If something arrived, receive it.
        if (fds->revents & POLLIN) {
			struct mdp_header mdp_recv_header;

			// Set the playoadsize.
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
				// If we reveived the result, copy it to the result array.
	            pinfo("Answer received.");
	            memcpy(rpc_result, &recv_payload[2], incoming_len - 2);
	            received = 2;
	        }
		}
	}

	// Cleanup.
    mdp_close(mdp_sockfd);

	return received;
}
