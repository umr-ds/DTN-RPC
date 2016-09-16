#include "rpc.h"

/******* Function where the server checks if the call should be accpected. ****/
/******* Implement acceptance predicates here. ********************************/
/******* Return 1 if should accpet, 0 otherwise. ******************************/
int _rpc_server_accepts (struct RPCProcedure *rp, uint32_t requirements_raw) {
	pinfo("Checking, if should accept call \"%s\".", rp->name);
    int requirements[8];
    _rpc_server_parse_requirements(requirements, requirements_raw);

    // Write your code here.

	return 1;
}

// Function to parse 4 byte number to 4 bit numbers for requirements.
// Illustration with example
// raw: ...01100110 &
//         00001111
// ------------------
// res: ...00000110
// Shift away last 4 bits and keep going.
void _rpc_server_parse_requirements (int *result, uint32_t raw) {
    uint32_t raw_copy = raw;
    int i;
    for (i = 0; i < 8; i++) {
        result[i] = raw_copy & 15;
        raw_copy = raw_copy >> 4;
    }

    int j;
    for (i = 0, j = 0; i < j; i++, j--) {
        int tmp = result[i];
        result[i] = result[j];
        result[j] = tmp;
    }
}

// Function to check, if a RPC is offered by this server.
// Load and parse the rpc.conf file.
int _rpc_server_offering (struct RPCProcedure *rp) {
    _rpc_eval_event(0, "checking offered", rp->caller_sid);

    pinfo("Checking, if \"%s\" is offered.", rp->name);
    // Build the path to the rpc.conf file and open it.
    int path_size = strlen(SYSCONFDIR) + strlen(SERVAL_FOLDER) + strlen(RPC_CONF_FILENAME) + 1;
    char path[path_size];
    memset(path, 0, path_size);
    FORMF_SERVAL_ETC_PATH(path, RPC_CONF_FILENAME);
    FILE *conf_file = fopen(path, "r");

    char *line = NULL;
    size_t len = 0;
    int ret = 0;

    // Read the file line by line.
    while (getline(&line, &len, conf_file) != -1) {
        ret = 0;
        // Split the line at the first space to get the return type.
        char *name = strtok(line, " ");
        // If the name matches with the received name ...
        if (!strncmp(name, rp->name, strlen(name))) {
            ret = 1;
        }

        // Split the line at the second space to get the paramc.
        char *paramc = strtok(NULL, " ");
		// ... and the parameter count, the server offers this RPC.
        if (ret && !strncmp(paramc, rp->paramc.paramc_s, strlen(paramc))) {
            ret = 2;
			break;
        }
    }

    // Cleanup.
    if (line) {
        free(line);
    }
    fclose(conf_file);

    return ret;
}

// Function to parse the received payload.
struct RPCProcedure _rpc_server_parse_call (uint8_t *payload, size_t len) {
    _rpc_eval_event(0, "parsing call", SID_ANY);
    pinfo("Parsing call.");
    // Create a new rp struct.
    struct RPCProcedure rp;

    // Parse the parameter count.
    rp.paramc.paramc_n = read_uint8(&payload[5]);
    // uint8 -> 2⁸ = 256 = 3 chars = 4 chars with \0, so allocate 4 bytes.
	rp.paramc.paramc_s = calloc(4, sizeof(char));
	sprintf(rp.paramc.paramc_s, "%u", read_uint8(&payload[5]));

    // Cast the payload starting at byte 7 to string.
    // The first 6 bytes are for packet type, param count and requirement list.
    char ch_payload[len - 6];
    memcpy(ch_payload, &payload[6], len - 6);

    // Split the payload at the first '|'
    char *tok = strtok(ch_payload, "|");

    // Set the name of the procedure.
    rp.name = calloc(strlen(tok) + 1, sizeof(char));
    strncpy(rp.name, tok, strlen(tok));
    strncpy(&rp.name[strlen(tok)], "\0", 1);

    // Allocate memory for the parameters and split the remaining payload at '|'
    // until it's it fully consumed. Store the parameters as strings in the designated struct field.
    int i = 0;

    rp.params = calloc(rp.paramc.paramc_n, sizeof(char*));
    tok = strtok(NULL, "|");
    while (tok) {
        rp.params[i] = calloc(strlen(tok) + 1, sizeof(char));
        strcpy(rp.params[i++], tok);
        tok = strtok(NULL, "|");
    }

    return rp;
}

// Execute the procedure
int _rpc_server_excecute (uint8_t *result_payload, struct RPCProcedure rp) {
    _rpc_eval_event(0, "executing", rp.caller_sid);
    pinfo("Executing \"%s\".", rp.name);
    FILE *pipe_fp;

    // Compile the rpc name and the path of the binary to one string.
    char bin[strlen(SYSCONFDIR) + strlen(BIN_FOLDER) + strlen(rp.name)];
    sprintf(bin, "%s%s%s", SYSCONFDIR, BIN_FOLDER, rp.name);

	// Since we use popen, which expects a string where the binary with all parameters delimited by spaces is stored,
	// we have to compile the bin with all parameters from the struct.
	// If this is an complex call, we have to donwload the file form the store and replace the hash with the path to the file.
	if (_rpc_str_is_filehash(rp.params[0])) {
		char fpath[128 + strlen(rp.name) + 3];
		while (_rpc_download_file(fpath, rp.name, alloca_tohex_sid_t(rp.caller_sid)) != 1) sleep(1);

		free(rp.params[0]);
		rp.params[0] = calloc(strlen(fpath) + 1, sizeof(char));
		strcpy(rp.params[0], fpath);
	}
	char *flat_params = _rpc_flatten_params(rp.paramc.paramc_n, (char **) rp.params, " ");

    char cmd[strlen(bin) + strlen(flat_params)];
    sprintf(cmd, "%s%s", bin, flat_params);

    // Open the pipe.
    if ((pipe_fp = popen(cmd, "r")) == NULL) {
        pfatal("Could not open the pipe. Aborting.");
        return 0;
    }

    // Payload. Two bytes for packet type, 126 bytes for the result and 1 byte for '\0' to make sure the result will be a zero terminated string.
    write_uint8(&result_payload[0], RPC_PKT_CALL_RESPONSE);

    // If the pipe is open ...
    if (pipe_fp) {
        // ... read the result, store it in the payload ...
        char *UNUSED(tmp_fgets_res) = fgets((char *)&result_payload[1], RPC_PKT_SIZE-1, pipe_fp);
		memcpy(&result_payload[RPC_PKT_SIZE], "\0", 1);

		if (!access((char *) &result_payload[1], F_OK)) {
			// Add the file to the Rhizome store given as the second parameter and replace the local path with the filehash.
			char filehash[RPC_PKT_SIZE];
			_rpc_add_file_to_store(filehash, rp.caller_sid, rp.name, (char*) &result_payload[1]);

			memcpy(&result_payload[1], filehash, RPC_PKT_SIZE-1);
			memcpy(&result_payload[RPC_PKT_SIZE], "\0", 1);
		}

        // ... and close the pipe.
        int ret_code = pclose(pipe_fp);
        if (WEXITSTATUS(ret_code) != 0) {
            pfatal("Execution of \"%s\" went wrong. See errormessages above for more information. Status %i.", flat_params, WEXITSTATUS(ret_code));
            return 0;
        }
        pinfo("Returned result from Binary.");
        free(flat_params);
    } else {
        return 0;
    }
    return 1;
}

// Main listening function.
int rpc_server_listen () {
    _rpc_eval_event(0, "listen start", SID_ANY);

	server_mode = RPC_SERVER_MODE_ALL;
	// Setup MDP and MSP.
    if (_rpc_server_msp_setup() == -1) {
		pfatal("Could not setup MSP listener. Aborting.");
		return -1;
	}
	if (_rpc_server_mdp_setup() == -1) {
		pfatal("Could not setup MDP listener. Aborting.");
		return -1;
	}
    // Run RPC server.
    while (server_running < 2) {
        if (server_running == 1) {
			// Clean everythin up.
			_rpc_server_msp_cleanup();
			_rpc_server_mdp_cleanup();
            break;
        }
		// Process the three main parts.
		_rpc_server_msp_process();
		_rpc_server_mdp_process();
        if (_rpc_server_rhizome_process() == -1) {
			pfatal("Rhizome listening failed. Aborting.");
			server_running = 1;
		}
        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);
    }
    _rpc_eval_event(0, "listen stop", SID_ANY);
	return 0;
}

// MDP listening function.
int rpc_server_listen_msp () {
    _rpc_eval_event(0, "listen start MSP", SID_ANY);

	server_mode = RPC_SERVER_MODE_MSP;
	// Setup MSP.
    if (_rpc_server_msp_setup() == -1) {
		pfatal("Could not setup MSP listener. Aborting.");
		return -1;
	}
    // Run RPC server.
    while (server_running < 2) {
        if (server_running == 1) {
			// Clean everythin up.
			_rpc_server_msp_cleanup();
            break;
        }
		// Process MSP.
		_rpc_server_msp_process();
        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);
    }
    _rpc_eval_event(0, "listen stop MSP", SID_ANY);
	return 0;
}

// Rhizome listening function.
int rpc_server_listen_rhizome () {
    _rpc_eval_event(0, "listen start Rhizome", SID_ANY);

	server_mode = RPC_SERVER_MODE_RHIZOME;
	// Run RPC server.
    while (server_running < 1) {
		// Process Rhizome
        if (_rpc_server_rhizome_process() == -1) {
			pfatal("Rhizome listening failed. Aborting.");
			server_running = 1;
		}
        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);
    }
    _rpc_eval_event(0, "listen stop Rhizome", SID_ANY);
	return 0;
}

// MDP listening function.
int rpc_server_listen_mdp_broadcast () {
    _rpc_eval_event(0, "listen start MDP", SID_ANY);

	server_mode = RPC_SERVER_MODE_MDP;
	// Setup MDP.
	if (_rpc_server_mdp_setup() == -1) {
		pfatal("Could not setup MDP listener. Aborting.");
		return -1;
	}
    // Run RPC server.
    while (server_running < 2) {
        if (server_running == 1) {
			// Clean everythin up.
			_rpc_server_mdp_cleanup();
            break;
        }
		// Process MDP.
		_rpc_server_mdp_process();
        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);
    }
    _rpc_eval_event(0, "listen stop MDP", SID_ANY);
	return 0;
}
