#include "rpc.h"

// This is needed, since this string is defined in another main.c. If we would link against this main.c, we would have trouble,
// since it is not possible to have multiple definitions of the same method, especially multiple main methods.
char crash_handler_clue[1024] = "no clue";

// Small function to open the keyring.
void _open_keyring () {
    keyring = keyring_open_instance("");
    keyring_enter_pin(keyring, "");
}

// Small function to close the keyring.
void _close_keyring () {
    keyring_free(keyring);
    keyring = NULL;
}

int main (int argc, char **argv) {
	// First, check if servald is running.
	if (server_pid() == 0) {
		printf("RPC WARN: Servald not running. Aborting.\n");
		return -1;
	}

	// Make sure all required params are set.
	if (argc < 2) {
		printf("RPC WARN: Not enough arguments. Aborting.\n"
				"Usage for starting the RPC server: %s listen\n"
				"Usage for starting the RPC client: %s call <server_sid> <procedure> <arg_1> [<arg_2> ...]\n",
				argv[0], argv[0]);
		return -1;
	}

    _open_keyring();

	// This is the server part. We just listen.
	if (strncmp(argv[1], "listen", strlen("listen")) == 0) {
		return rpc_listen();
	}
	else {
		// Parse params.
		const char *sidhex = argv[2];
		const char *name = argv[3];
		const char *param1 = argv[4];

		sid_t sid;
		if (str_to_sid_t(&sid, sidhex) == -1){
			printf("RPC WARN: str_to_sid_t() failed");
			return -1;
		}

		// Get length of additional arguments...
		unsigned int nfields = (argc == 5) ? 0 : argc - 5;
		// and create new parameter array of the particular length.
		const char *params[nfields + 1];
		params[0] = param1;
		// Parse additional arguments:
		unsigned int i;
		for (i = 0; i < nfields; i++) {
			// Skip to next parameter and save it in params at position i.
			unsigned int n = nfields + i + 4;
			params[i + 1] = argv[n];
		}

		int ret_code = rpc_call(sid, name, nfields + 1, params);
		if (ret_code == 0) {
			printf("RPC DEBUG: RPC result: %s\n", (char *) rpc_result);
			return 0;
		}
		printf("RPC WARN: Something went wrong. No result.\n");
		return -1;
	}
    _close_keyring();
}
