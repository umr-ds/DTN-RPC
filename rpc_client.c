#include "rpc.h"

// General call function. For transparent usage.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params) {
	received = 0;
    if (is_sid_t_broadcast(server_sid)) {
		// Broadcast the RPC.
		int call_return = rpc_client_call_mdp_broadcast(rpc_name, paramc, params);

		if (call_return == -1) {
			// If MDP ist not possible, try Rhizome.
	        pwarn("No Server found. Trying Rhizome.");
			return -1;
        } if (call_return == 0) {
            pfatal("Seems like the server accepted the call, but could not receive ACK or result. Aborting.");
            return -1;
        } if (call_return == 1) {
            pfatal("Received ACK but could not collect result. Aborting.");
            return -1;
        }
        return call_return;
    }

	else {
        // Call the rpc directly over msp.
        int call_return = rpc_client_call_msp(server_sid, rpc_name, paramc, params);

        if (call_return == -1) {
			// If MSP ist not possible, try Rhizome.
	        pwarn("Server not available via MSP. Trying Rhizome.");
            call_return = rpc_client_call_rhizome(server_sid, rpc_name, paramc, params);
			if (call_return == -1) {
				pfatal("Call via Rhizome was not successfull. RPC not available. Try later.");
				return -1;
			}
        } if (call_return == 0) {
            pfatal("Seems like the server accepted the call, but could not receive ACK or result. Aborting.");
            return -1;
        } if (call_return == 1) {
            pfatal("Received ACK but could not collect result. Aborting.");
            return -1;
        }
		return call_return;
    }

    return 1;
}
