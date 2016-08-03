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

void _rpc_server_sig_handler (int signum) {
    pwarn("Caught signal with signum %i. Stopping RPC server.", signum);
    running = 1;
}

void _rpc_print_usage (int mode, char *reason) {
	switch (mode) {
		case 1:
			pfatal("%s\nUsage (server): ./servalrpc (--listen | -l) [(--msp | -s) | (--mdp | -d) | (--rhizome | -r)]", reason);
			break;
		case 2:
			pfatal("%s\nUsage (client): ./servalrpc (--call | -c) [(--msp | -s) | (--mdp | -d) | (--rhizome | -r)] -- (<server_sid> | broadcast | any) <procedure> <arg_1> [<arg_2> ...]", reason);
			break;
		default:
			pfatal( "%s\n"
					"Usage (server): ./servalrpc (--listen | -l) [(--msp | -s) | (--mdp | -d) | (--rhizome | -r)]\n"
					"Usage (client): ./servalrpc (--call | -c) [(--msp | -s) | (--mdp | -d) | (--rhizome | -r)] -- (<server_sid> | broadcast | any) <procedure> <arg_1> [<arg_2> ...]",
					reason
			);
	}
}

int _rpc_check_cli (char *arg, char *option, char *abbrev) {
	char lng_option[2 + strlen(option)];
	sprintf(lng_option, "--%s", option);
	char lng_abbrev[1 + strlen(abbrev)];
	sprintf(lng_abbrev, "-%s", abbrev);
	int option_res = strncmp(arg, lng_option, strlen(lng_option)) == 0;
	int abbrev_res = strncmp(arg, lng_abbrev, strlen(lng_abbrev)) == 0;

	return option_res || abbrev_res;
}

int main (int argc, char **argv) {
	// First, check if servald is running.
	if (server_pid() == 0) {
		pfatal("Servald not running. Aborting.");
		return -1;
	}

	// Make sure all required params are set.
	if (argc < 2) {
		_rpc_print_usage(0, "Not enough arguments!");
		return -1;
	}

    _open_keyring();

	// This is the server part. We just listen.
	if (_rpc_check_cli(argv[1], "listen", "l")) {
    	signal(SIGINT, _rpc_server_sig_handler);
    	signal(SIGTERM, _rpc_server_sig_handler);
		if (argc == 3) {
			if (_rpc_check_cli(argv[2], "msp", "s")) {
				pinfo("Server mode: MSP");
				return rpc_server_listen_msp();
			} else if (_rpc_check_cli(argv[2], "mdp", "d")) {
				pinfo("Server mode: MDP");
				return rpc_server_listen_mdp_broadcast();
			} else if (_rpc_check_cli(argv[2], "rhizome", "r")) {
				pinfo("Server mode: Rhizome");
				return rpc_server_listen_rhizome();
			} else {
				_rpc_print_usage(1, "Unrecognized option.");
				return -1;
			}
		} else if (argc == 2) {
			pinfo("Server mode: All");
			return rpc_server_listen();
		} else {
			_rpc_print_usage(1, "Too many options.");
		}
	} else if (_rpc_check_cli(argv[1], "call", "c")) {
		int offset = 0;
		if (strcmp(argv[2], "--") == 0) {
			offset = 3;
		} else {
			offset = 4;
		}

		// Parse params.
		const char *sidhex = argv[offset];
		const char *name = argv[offset + 1];
		const char *param1 = argv[offset + 2];

		if (strncmp(sidhex, "any", strlen("any")) == 0) {
			sidhex = "broadcast";
		}
		sid_t sid;
		if (str_to_sid_t(&sid, sidhex) == -1) {
			pfatal("Could not convert SID to sid_t. Aborting.");
			return -1;
		}

		// Get length of additional arguments...
		unsigned int nfields = argc - (offset + 3);
		// and create new parameter array of the particular length.
		const char *params[nfields + 1];
		params[0] = param1;
		// Parse additional arguments:
		unsigned int i;
		for (i = 0; i < nfields; i++) {
			// Skip to next parameter and save it in params at position i.
			unsigned int n = offset + 3 + i;
			params[i + 1] = argv[n];
		}

		int ret_code = -1;
		if (_rpc_check_cli(argv[2], "msp", "s")) {
			pinfo("Client mode: MSP (direct)");
			ret_code = rpc_client_call_msp(sid, name, nfields + 1, params);
		} else if (_rpc_check_cli(argv[2], "mdp", "d")) {
			pinfo("Client mode: MDP (broadcasts)");
			ret_code = rpc_client_call_mdp_broadcast(name, nfields + 1, params);
		} else if (_rpc_check_cli(argv[2], "rhizome", "r")) {
			pinfo("Client mode: Rhizome (delay-tolerant)");
			ret_code = rpc_client_call_rhizome(sid, name, nfields + 1, params);
		} else if (_rpc_check_cli(argv[2], "-", "-")) {
			pinfo("Client mode: Transparent.");
			ret_code = rpc_call(sid, name, nfields + 1, params);
		} else {
			_rpc_print_usage(2, "Unrecognized option.");
		}

		if (ret_code == 2) {
			pinfo("RPC result: %s", (char *) rpc_result);
			return 0;
		}
		pfatal("Something went wrong. No result.");
		return -1;
	} else {
		_rpc_print_usage(0, "Unrecognized option.");
	}
    _close_keyring();
}
