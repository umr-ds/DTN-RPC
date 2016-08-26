#include "rpc.h"

time_ms_t next_time;
MSP_SOCKET sock_msp = MSP_SOCKET_NULL;
int mdp_fd_msp;
struct mdp_sockaddr addr_msp;

struct RPCProcedure rp;
size_t actual_size = 0;
char *fpath = NULL;

// Function where execution gets handled.
// It's a little different than in MDP or Rhizome since we do not use DTN filetranfer
// for MSP but the MSP connection itself.
int _rpc_server_msp_run_rp () {
	// If there is no procedure, skip.
	if (!rp.name) {
		return -1;
	}
	// Check, if we offer this procedure and we should accept the call.
	if (_rpc_server_offering(&rp) && _rpc_server_accepts(&rp)) {
		pinfo("Offering desired RPC. Sending ACK.");
		// Compile and send ACK packet.
		uint8_t ack_payload[2];
		write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
		msp_send(sock_msp, ack_payload, sizeof(ack_payload));

		// If the first parameter is "size:<size>", we have to wait until the
		// data arrives (similar to the complex case for MDP or Rhizome, where
		// the first parameter is "filehash" for complex calls.)
		if (!strncmp(rp.params[0], "size:", 5)) {
			// Get the size of the incoming data.
			char *size_str = &strstr(rp.params[0], ":")[1];
			size_t size = (size_t) atoi(size_str);
			// Process MSP until all data arrived.
			// TODO: If connection gets lost, abort.
			while (actual_size + 1 < size) {
				_rpc_server_msp_process();
			}

			// If the data arrived, remove the "size:<size>" part and replace it
			// with the path to file.
			free(rp.params[0]);
			rp.params[0] = calloc(strlen(fpath), sizeof(char));
			strcpy(rp.params[0], fpath);
		}

		// Try to execute the procedure.
		uint8_t result_payload[2 + 129 + 1];
		memset(result_payload, 0, 132);
		if (_rpc_server_excecute(result_payload, rp)) {
			// Try MSP
			if (!msp_socket_is_null(sock_msp) && msp_socket_is_data(sock_msp)) {
				pinfo("Sending result via MSP.");
				if (msp_send(sock_msp, result_payload, sizeof(result_payload)) == sizeof(result_payload)) {
					pinfo("RPC execution was successful.");
				}
			} else if (server_mode == RPC_SERVER_MODE_ALL) {
				// Use Rhizome if MSP is not available and server_mode is ALL.
				pwarn("MSP not available for result. Sending via Rhizome.");
				_rpc_server_rhizome_send_result(rp.caller_sid, rp.name, result_payload);
			} else {
				pfatal("MSP not available for result. Aborting.");
			}
		} else {
			pfatal("RPC execution was not successful. Aborting.");
		}
	} else {
		pwarn("Not offering desired RPC. Ignoring.");
	}
	// Cleanup.
	_rpc_free_rp(rp);
	rp.name = NULL;
	sock_msp = MSP_SOCKET_NULL;
	return 1;
}

// Handler of the RPC server
size_t _rpc_server_msp_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_SHUTDOWN_REMOTE | MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        pwarn("Socket closed.");
        msp_stop(sock);
		return len;
    }

    // If we receive something, handle it.
    if (payload && len) {
        // First make sure, we received a RPC call packet.
        if (read_uint16(&payload[0]) == RPC_PKT_CALL) {
            pinfo("Received RPC via MSP.");
			// Get the remote address.
			struct mdp_sockaddr addr;
		    bzero(&addr, sizeof addr);
			msp_get_remote(sock, &addr);
            // Parse the payload to the RPCProcedure struct
            rp = _rpc_server_parse_call((uint8_t *) payload, len);
			if (str_to_sid_t(&rp.caller_sid, alloca_tohex_sid_t(addr.sid)) == -1) {
				pfatal("Could not convert SID to sid_t. Aborting.");
				return len;
			}
			// Set the global socket to the current.
			sock_msp = sock;
			return len;
        }
		// If we get a chunk packet, store the chunk to a file.
		else if (read_uint16(&payload[0]) == RPC_PKT_CALL_CHUNK) {
			// First, get sender and send time from the packet.
			char *sender = alloca_tohex_sid_t(rp.caller_sid);
			uint64_t send_time = read_uint64(&payload[2]);
			// With this information, make the filename.
			char rpc_down_name[4 + strlen(rp.name) + sizeof(sender)];
			sprintf(rpc_down_name, "f_%s_%" PRIu64 "_%s", rp.name, send_time, sender);
			// With the filename, make the absolute path to the file.
			fpath = realloc(fpath, strlen(RPC_TMP_FOLDER) + strlen(rpc_down_name));
			sprintf(fpath, "%s%s", RPC_TMP_FOLDER, rpc_down_name);
			// Make sure the folder exists.
			mkdir(RPC_TMP_FOLDER, 0700);

			// Open the file.
			FILE* rpc_file = fopen(fpath, "a");
			// Write the chunk to that file and store the total downloaded size.
			actual_size = actual_size + fwrite(&payload[10], 1, len-10, rpc_file);
			// Close the file.
			fclose(rpc_file);
			return len;
		}
    }

    return len;
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

