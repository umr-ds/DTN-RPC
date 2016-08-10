#include "rpc.h"

time_ms_t next_time;
MSP_SOCKET sock_msp = MSP_SOCKET_NULL;
int mdp_fd_msp;
struct mdp_sockaddr addr_msp;

// Handler of the RPC server
size_t _rpc_server_msp_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    size_t ret = 0;

    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_SHUTDOWN_REMOTE | MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        pwarn("Socket closed.");
        msp_stop(sock);
        ret = len;
    }

    // If we receive something, handle it.
    if (payload && len) {
        // First make sure, we received a RPC call packet.
        if (read_uint16(&payload[0]) == RPC_PKT_CALL) {
            pinfo("Received RPC via MSP.");
            // Parse the payload to the RPCProcedure struct
			struct mdp_sockaddr addr;
		    bzero(&addr, sizeof addr);
			msp_get_remote(sock, &addr);
            struct RPCProcedure rp = _rpc_server_parse_call((uint8_t *) payload, len);
			if (str_to_sid_t(&rp.caller_sid, alloca_tohex_sid_t(addr.sid)) == -1) {
				pfatal("Could not convert SID to sid_t. Aborting.");
				return len;
			}

            // Check, if we offer this procedure and we should accept the call.
            if (_rpc_server_offering(&rp) && _rpc_server_accepts(&rp)) {
                pinfo("Offering desired RPC. Sending ACK.");
                // Compile and send ACK packet.
                uint8_t ack_payload[2];
                write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
                ret = msp_send(sock, ack_payload, sizeof(ack_payload));

                // Try to execute the procedure.
			    uint8_t result_payload[2 + 129 + 1];
                if (_rpc_server_excecute(result_payload, rp) == 0) {
					// Try MSP
					if (!msp_socket_is_null(sock) && msp_socket_is_data(sock)){
						pinfo("Sending result via MSP.");
	        			if (msp_send(sock, result_payload, sizeof(result_payload)) == sizeof(result_payload)) {
							pinfo("RPC execution was successful.");
						}
					} else if (server_mode == RPC_SERVER_MODE_ALL) {
						// Use Rhizome if MSP is not available and server_mode is ALL.
						pwarn("MSP not available for result. Sending via Rhizome.");
						_rpc_server_rhizome_send_result(rp.caller_sid, rp.name, result_payload);
					} else {
						pfatal("MSP not available for result. Aborting.");
					}
                    ret = len;
                } else {
					pfatal("RPC execution was not successful. Aborting.");
                    ret = len;
                }
            } else {
                pwarn("Not offering desired RPC. Ignoring.");
                ret = len;
            }
        }
    }
    return ret;
}

// Setup the MSP part.
int _rpc_server_msp_setup () {
	// Init the address struct and set the sid to a local sid and port where to listen at.
    bzero(&addr_msp, sizeof addr_msp);
    addr_msp.sid = BIND_PRIMARY;
    addr_msp.port = MDP_PORT_RPC_MSP;

    // Create MDP socket.
	if ((mdp_fd_msp = mdp_socket()) < 0) {
		pfatal("Could not create MDP listening socket for MSP. Aborting.");
		return -1;
	}

    // Sockets should not block.
    set_nonblock(mdp_fd_msp);
    set_nonblock(STDIN_FILENO);
    set_nonblock(STDERR_FILENO);
    set_nonblock(STDOUT_FILENO);

    // Create MSP socket.
    sock_msp = msp_socket(mdp_fd_msp, 0);
    // Connect to local socker ...
    msp_set_local(sock_msp, &addr_msp);
    // ... and listen it.
    msp_listen(sock_msp);

    // Set the handler to handle incoming packets.
    msp_set_handler(sock_msp, _rpc_server_msp_handler, NULL);

	return 0;
}

// MSP listener.
void _rpc_server_msp_process () {
	// Process MSP socket.
	msp_processing(&next_time);
	// Receive the data from the socket.
	msp_recv(mdp_fd_msp);
}

// MSP Cleanup.
void _rpc_server_msp_cleanup () {
	sock_msp = MSP_SOCKET_NULL;
	msp_close_all(mdp_fd_msp);
	mdp_close(mdp_fd_msp);
	msp_processing(&next_time);
}
