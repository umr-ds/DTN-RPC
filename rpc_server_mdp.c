#include "rpc.h"

int mdp_sock;
struct mdp_sockaddr mdp_addr;
struct pollfd mdp_poll_fd[2];

// Setup the MDP server part.
int _rpc_server_mdp_setup () {
	// Open the socket.
	if ((mdp_sock = mdp_socket()) < 0) {
		pfatal("Could not create MDP listening socket. Aborting.");
		return -1;
	}

	// Set address (SID) and port (MDP port).
	mdp_addr.sid = BIND_PRIMARY;
	mdp_addr.port = MDP_PORT_RPC;

	// Bind to the open socket.
	if (mdp_bind(mdp_sock, &mdp_addr) < 0) {
		pfatal("Could not bind to broadcast address.");
		return -1;
	}

	// Setup the poll fd.
	mdp_poll_fd[0].fd = mdp_sock;
	mdp_poll_fd[0].events = POLLIN;

	return 0;
}

// Function for handling incoming MDP packets.
static int _rpc_server_mdp_handle (int mdp_sockfd) {
	// Setup MDP header where meta data from incoming packet gets stored.
	struct mdp_header header;
	uint8_t payload[1200];

	// Receive payload.
	ssize_t len = mdp_recv(mdp_sockfd, &header, payload, sizeof(payload));
	if (len == -1) {
		pwarn("Could not receive MDP payload.");
		return -1;
	}

	// At this point the header is constucted so we can set our address for replies.
	header.local.sid = my_subscriber->sid;

	// If the packet is a RPC call, handle it.
	if (read_uint16(&payload[0]) == RPC_PKT_CALL) {
		pinfo("Received RPC call via MDP broadcast.");
		// Parse the payload to the RPCProcedure struct
		struct RPCProcedure rp = _rpc_server_parse_call(payload, len);
		if (str_to_sid_t(&rp.caller_sid, alloca_tohex_sid_t(header.remote.sid)) == -1) {
			pfatal("Could not convert SID to sid_t. Aborting.");
			return -1;
		}

		// Check, if we offer this procedure.
        if (_rpc_server_check_offered(&rp) == 0) {
            pinfo("Offering desired RPC. Sending ACK.");
            // Compile and send ACK packet.
            uint8_t ack_payload[2];
            write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
            mdp_send(mdp_sockfd, &header, ack_payload, sizeof(ack_payload));

            // Try to execute the procedure.
			uint8_t result_payload[2 + 129 + 1];
			if (_rpc_server_excecute(result_payload, rp) == 0) {
				// Send result back after successful execution.
				if (_rpc_sid_is_reachable(header.remote.sid)){
					// Try MDP
					pinfo("Sending result via MDP.");
					mdp_send(mdp_sockfd, &header, result_payload, sizeof(result_payload));
				} else if (server_mode == RPC_SERVER_MODE_ALL) {
					// Use Rhizome if MDP is not available and server_mode is ALL.
					pwarn("MDP not available for result. Sending via Rhizome.");
					_rpc_server_rhizome_send_result(SID_BROADCAST, rp.name, result_payload);
				} else {
					pfatal("MDP not available for result. Aborting.");
				}
                pinfo("RPC execution was successful.");
            }
        } else {
            pwarn("Not offering desired RPC. Ignoring.");
        }
    }

	return 0;
}

// MDP listener.
void _rpc_server_mdp_process () {
	poll(mdp_poll_fd, 1, 500);

	// If we have some data, handle the data.
	if (mdp_poll_fd->revents & POLLIN) {
		_rpc_server_mdp_handle(mdp_sock);
	}
}

// Close MDP socket.
void _rpc_server_mdp_cleanup () {
	mdp_close(mdp_sock);
}
