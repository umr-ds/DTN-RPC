#include "rpc.h"

// Prepare the payload whith the RPC information.
uint8_t *_rpc_client_prepare_call_payload (uint8_t *payload, int paramc, char *rpc_name, char *flat_params) {
        // Write the packettype, ...
        write_uint16(&payload[0], RPC_PKT_CALL);
        // ... number of parameters, ...
        write_uint16(&payload[2], (uint16_t) paramc);
        // ... the RPC name ...
        memcpy(&payload[4], rpc_name, strlen(rpc_name));
        // ... and the parameters.
        memcpy(&payload[4 + strlen(rpc_name)], flat_params, strlen(flat_params));
        // Make sure there is a string terminater. Makes it easier to parse on server side.
        memcpy(&payload[4 + strlen(rpc_name) + strlen(flat_params)], "\0", 1);

        return payload;
}

// Function to flatten the parameters and replace the first parameter if it is a local path.
int _rpc_client_replace_if_path (char *flat_params, char *rpc_name, char **params, int paramc) {
	if (!access(params[0], F_OK)) {
		// Add the file to the Rhizome store given as the first parameter and replace the local path with the filehash.
		char filehash[129];
		_rpc_add_file_to_store(filehash, SID_BROADCAST, rpc_name, params[0]);

		char *new_params[paramc];
		new_params[0] = filehash;

		int i;
		for (i = 1; i < paramc; i++) {
			new_params[i] = (char *) params[i];
		}

		char *flat = _rpc_flatten_params(paramc, (char **) new_params, "|");
		strcpy(flat_params, flat);
	} else {
		char *flat = _rpc_flatten_params(paramc, params, "|");
		strcpy(flat_params, flat);
	}
	return 0;
}

// General call function. For transparent usage.
int rpc_client_call (sid_t server_sid, char *rpc_name, int paramc, char **params) {
	received = 0;
    if (is_sid_t_broadcast(server_sid)) {
		// Broadcast the RPC.
		int call_return = rpc_client_call_mdp_broadcast(rpc_name, paramc, params);

		if (call_return == -1) {
			// If MDP ist not possible, try Rhizome.
	        pwarn("No Server found via MDP. Trying Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
        }

		if (call_return == 0) {
			// Seems like the prior call was succesfull, but in the middle something went wrong. Trying again.
            pfatal("Seems like the server accepted the call via MDP, but the client could not receive ACK or result. Trying Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 0) {
				pfatal("Seems like the server accepted the call via Rhizome, but the client could not receive ACK or result. RPC not available. Try later.");
				return -1;
			}
        }

		if (call_return == 1) {
            pfatal("Received ACK but could not collect result. Last try via Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 0) {
				pfatal("Seems like the server accepted the call via Rhizome, but the client could not receive ACK or result. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 1) {
				pfatal("Received ACK via Rhizome but the client could not collect the result. RPC not available. Try later.");
				return -1;
			}
        }
        return call_return;
    }

	else {
        // Call the rpc directly over msp.
		client_mode = RPC_CLIENT_MODE_TRANSPARTEN;
        int call_return = rpc_client_call_msp(server_sid, rpc_name, paramc, params);

		if (call_return == -1) {
			// If MSP ist not possible, try Rhizome.
	        pwarn("No Server found via MSP. Trying Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
        }

		if (call_return == 0) {
			// Seems like the prior call was succesfull, but in the middle something went wrong. Trying again.
            pfatal("Seems like the server accepted the call via MSP, but the client could not receive ACK or result. Trying Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 0) {
				pfatal("Seems like the server accepted the call via Rhizome, but the client could not receive ACK or result. RPC not available. Try later.");
				return -1;
			}
        }

		if (call_return == 1) {
            pfatal("Received ACK but could not collect result. Last try via Rhizome.");

			call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);

			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 0) {
				pfatal("Seems like the server accepted the call via Rhizome, but the client could not receive ACK or result. RPC not available. Try later.");
				return -1;
			}
			if (call_return == 1) {
				pfatal("Received ACK via Rhizome but the client could not collect the result. RPC not available. Try later.");
				return -1;
			}
        }
        return call_return;
    }

    return 1;
}
