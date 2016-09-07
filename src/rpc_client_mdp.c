#include "rpc.h"

// MDP call function.
int rpc_client_call_mdp (sid_t server_sid, char *rpc_name, int paramc, char **params, uint32_t requirements) {
	// If the server_sid is not broadcast or any, we have to check, if the server is available.
	if (!is_sid_t_any(server_sid) && !is_sid_t_broadcast(server_sid) && !_rpc_sid_is_reachable(server_sid)) {
		pfatal("Server %s not reachable. Aborting.", alloca_tohex_sid_t(server_sid));
		return -1;
	}

	// Make sure the result array is empty.
	memset(rpc_result, 0, sizeof(rpc_result));
	// Open the mdp socket.
	int mdp_sockfd;
	if ((mdp_sockfd = mdp_socket()) < 0) {
		return WHY("Cannot create MDP socket");
	}

	// Set my own local SID and the MDP port for RPCs.
	// If we get the SID_ANY SID, we set the broadcast SID.
	// Otherwise set the SID we got. If it is SID_BROADCAST it will be the right SID implicitly.
	struct mdp_header mdp_header;
	bzero(&mdp_header, sizeof(mdp_header));
	mdp_header.local.sid = BIND_PRIMARY;
	if (is_sid_t_any(server_sid)) {
		mdp_header.remote.sid = BIND_ALL;
	} else {
		mdp_header.remote.sid = server_sid;
	}
	mdp_header.remote.port = MDP_PORT_RPC;
	if (is_sid_t_broadcast(server_sid) || is_sid_t_any(server_sid)) {
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
	uint8_t payload[1 + 4 + 1 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params, requirements);

	// pollfd for listening for the result via MDP.
	struct pollfd fds[2];
    fds->fd = mdp_sockfd;
    fds->events = POLLIN;

	time_t start_time = time(NULL);
	int wait_time = 10;
	while (received == 0 || received == 1) {

        // Wait for 10 seconds for answer (While develpment. Later maybe longer).
		if (time(NULL) - start_time > wait_time) {
            // If we hit the timeout, get the number of elements in the result array.
            int num_answers = rpc_client_result_get_insert_index();
            // If the SID is the broadcast id we check if we have at least one answer.
            if (is_sid_t_broadcast(server_sid) && num_answers > 0) {
                received = 2;
            } else {
                mdp_close(mdp_sockfd);
                return -1;
            }
		}

        // Send the call until we get at least one ack.
		if (received == 0) {
			// Send the payload.
			if (mdp_send(mdp_sockfd, &mdp_header, payload, sizeof(payload)) < 0) {
				pfatal("Could not send packet. Aborting.");
				return -1;
			}
		}

		// Poll the socket
        poll(fds, 1, 1000);

        // If something arrived, receive it.
        if (fds->revents & POLLIN) {
			struct mdp_header mdp_recv_header;

			// Set the payloadsize.
			uint8_t recv_payload[RPC_PKT_SIZE];
			ssize_t incoming_len = mdp_recv(mdp_sockfd, &mdp_recv_header, recv_payload, sizeof(recv_payload));

            // Skip empty packets.
			if (incoming_len < 0) {
				pwarn("Received empty packet. Ignoring.");
				continue;
			}

			// Get the packet type.
	        uint8_t pkt_type = read_uint8(&recv_payload[0]);
	        // If we receive an ACK, just print.
	        if (pkt_type == RPC_PKT_CALL_ACK) {
                // If this was an "all" call, and the server_sid is not in the result array yet, we store it.
                if (is_sid_t_broadcast(server_sid) && _rpc_client_result_get_sid_index(server_sid) == -1) {
                    int position = rpc_client_result_get_insert_index();
                    memcpy(&rpc_result[position].server_sid, &mdp_recv_header.remote.sid, sizeof(sid_t));
                }
				// Wait 20 seconds more if we receive ACK.
				wait_time += 20;
	            pinfo("Server %s accepted call.", alloca_tohex_sid_t(mdp_recv_header.remote.sid));
	            received = 1;
	        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
				// If we received the result, copy it to the result array:
	            pinfo("Answer received from %s.", alloca_tohex_sid_t(mdp_recv_header.remote.sid));
                // First, see if this SID has already an entry in the result array. If not, skip.
                int result_position = -1;
                if (is_sid_t_broadcast(server_sid)) {
                    result_position = _rpc_client_result_get_sid_index(mdp_recv_header.remote.sid);
                    if (result_position == -1) {
                        continue;
                    }
                }

                // If we get an filehash, we have to doneload the file.
                if (_rpc_str_is_filehash((char *) &recv_payload[1])) {
                    // Download the file and get the path.
                    char fpath[128 + strlen(rpc_name) + 3];
                    while (_rpc_download_file(fpath, rpc_name, alloca_tohex_sid_t(SID_BROADCAST)) != 0) sleep(1);
                    // If the result_position is -1, this was not a "all" call. So we have only one answer.
                    // We can finish.
                    if (result_position == -1) {
                        if (!is_sid_t_broadcast(server_sid)) {
                            memcpy(rpc_result[0].content, fpath, 128 + strlen(rpc_name) + 3);
                            memcpy(&rpc_result[0].server_sid, &mdp_recv_header.remote.sid, sizeof(sid_t));
                            received = 2;
                        }
                    } else {
                        // Otherwise store the result at the result_position.
                        memcpy(rpc_result[result_position].content, fpath, 128 + strlen(rpc_name) + 3);
                        // If the result array is full, we do not have to wait any longer and can finish execution.
                        if (result_position == 4) {
                            received = 2;
                        }
                    }
                } else {
                    // This is the simple case. Just store the payload.
                    // If the result_position is -1, this was not a "all" call. So we have only one answer.
                    // We can finish.
                    if (result_position == -1) {
                        if (!is_sid_t_broadcast(server_sid)) {
                            memcpy(&rpc_result[0].content, &recv_payload[1], incoming_len - 1);
                            memcpy(&rpc_result[0].server_sid, &mdp_recv_header.remote.sid, sizeof(sid_t));
                            received = 2;
                        }
                    } else {
                        // Otherwise store the result at the result_position.
                        memcpy(&rpc_result[result_position].content, &recv_payload[1], incoming_len - 1);
                        // If the result array is full, we do not have to wait any longer and can finish execution.
                        if (result_position == 4) {
                            received = 2;
                        }
                    }
                }
	        }
		}
	}

	// Cleanup.
    mdp_close(mdp_sockfd);

	return received;
}
