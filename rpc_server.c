#include "rpc.h"

#define RPC_CONF_FILENAME "rpc.conf"
#define SERVAL_FOLDER "/serval/"
#define BIN_FOLDER "/serval/rpc_bin/"

int running = 0;

int mdp_fd_msp;

// Function to check, if a RPC is offered by this server.
// Load and parse the rpc.conf file.
int rpc_check_offered (struct RPCProcedure *rp) {
    pinfo("Checking, if \"%s\" is offered.", rp->name);
    // Build the path to the rpc.conf file and open it.
    static char path[strlen(SYSCONFDIR) + strlen(SERVAL_FOLDER) + strlen(RPC_CONF_FILENAME) + 1] = "";
    FORMF_SERVAL_ETC_PATH(path, RPC_CONF_FILENAME);
    FILE *conf_file = fopen(path, "r");

    char *line = NULL;
    size_t len = 0;
    int ret = -1;

    // Read the file line by line.
    while (getline(&line, &len, conf_file) != -1) {
        // Split the line at the first space to get the return type.
        char *name = strtok(line, " ");
        // If the name matches with the received name ...
        if (strncmp(name, rp->name, strlen(name)) == 0) {
            ret = 1;
        }

        // Split the line at the second space to get the paramc.
        char *paramc = strtok(NULL, " ");
		// ... and the parameter count, the server offers this RPC.
        if (strncmp(paramc, rp->paramc.paramc_s, strlen(paramc)) == 0) {
            ret = 0;
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
struct RPCProcedure rpc_parse_call (const uint8_t *payload, size_t len) {
    pinfo("Parsing call.");
    // Create a new rp struct.
    struct RPCProcedure rp;

    // Parse the parameter count.
    rp.paramc.paramc_n = read_uint16(&payload[2]);
	rp.paramc.paramc_s = calloc(sizeof(char), 6);
	sprintf(rp.paramc.paramc_s, "%u", read_uint16(&payload[2]));

    // Cast the payload starting at byte 5 to string.
    // The first 4 bytes are for packet type and param count.
    char ch_payload[len - 4];
    memcpy(ch_payload, &payload[4], len - 4);

    // Split the payload at the first '|'
    char *tok = strtok(ch_payload, "|");

    // Set the name of the procedure.
    rp.name = calloc(strlen(tok), sizeof(char));
    strncpy(rp.name, tok, strlen(tok));

    // Allocate memory for the parameters and split the remaining payload at '|'
    // until it's it fully consumed. Store the parameters as strings in the designated struct field.
    int i = 0;

    rp.params = calloc(rp.paramc.paramc_n, sizeof(char*));
    tok = strtok(NULL, "|");
    while (tok) {
        rp.params[i] = calloc(strlen(tok), sizeof(char));
        strncpy(rp.params[i++], tok, strlen(tok));
        tok = strtok(NULL, "|");
    }
    return rp;
}
//
// int rpc_listen_msp () {
//
// }

// Send the result via Rhizome
int rpc_send_rhizome (const sid_t sid, const char *rpc_name, uint8_t *payload) {
    int return_code = 1;

    // Construct the manifest and write it to the manifest file.
    int manifest_size = strlen("service=RPC\nname=\nsender=\nrecipient=\n") + strlen(rpc_name) + (strlen(alloca_tohex_sid_t(sid)) * 2);
    char manifest_str[manifest_size];
    sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));
    char tmp_manifest_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));

    // Write the payload to the payload file.
    char tmp_payload_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_payload_file_name, payload, 130);

    // Init the cURL stuff.
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        return_code = -1;
        goto clean_rhizome_server_response;
    }

    // Declare all needed headers forms and URLs.
    struct curl_slist *header = NULL;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    char *url_insert = "http://localhost:4110/restful/rhizome/insert";

    // Set basic cURL options (see function).
    _curl_set_basic_opt(url_insert, curl_handler, header);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

    // Add the manifest and payload form and add the form to the cURL request.
    _curl_add_file_form(tmp_manifest_file_name, tmp_payload_file_name, curl_handler, formpost, lastptr);

    // Perfom request, which means insert the RPC file to the store.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (post): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_server_response_all;
    }

    // Clean up.
    clean_rhizome_server_response_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_server_response:
        _curl_free_memory(&curl_result_memory);
        remove(tmp_manifest_file_name);
        remove(tmp_payload_file_name);

    return return_code;
}

// Execute the procedure
int rpc_excecute (uint8_t *result_payload, struct RPCProcedure rp) {
    pinfo("Executing \"%s\".", rp.name);
    FILE *pipe_fp;

    // Compile the rpc name and the path of the binary to one string.
    char bin[strlen(SYSCONFDIR) + strlen(BIN_FOLDER) + strlen(rp.name)];
    sprintf(bin, "%s%s%s", SYSCONFDIR, BIN_FOLDER, rp.name);

    // Since we use popen, which expects a string where the binary with all parameters delimited by spaces is stored,
    // we have to compile the bin with all parameters from the struct.
    char *flat_params = _rpc_flatten_params(rp.paramc.paramc_n, (const char **) rp.params, " ");

    char cmd[strlen(bin) + strlen(flat_params)];
    sprintf(cmd, "%s%s", bin, flat_params);

    // Open the pipe.
    if ((pipe_fp = popen(cmd, "r")) == NULL) {
        pfatal("Could not open the pipe. Aborting.");
        return -1;
    }

    // Payload. Two bytes for packet type, 126 bytes for the result and 1 byte for '\0' to make sure the result will be a zero terminated string.
    write_uint16(&result_payload[0], RPC_PKT_CALL_RESPONSE);

    // If the pipe is open ...
    if (pipe_fp) {
        // ... read the result, store it in the payload ...
        fgets((char *)&result_payload[2], 127, pipe_fp);
        memcpy(&result_payload[129], "\0", 1);
        // ... and close the pipe.
        int ret_code = pclose(pipe_fp);
        if (WEXITSTATUS(ret_code) != 0) {
            pfatal("Execution of \"%s\" went wrong. See errormessages above for more information. Status %i.", flat_params, WEXITSTATUS(ret_code));
            return -1;
        }
        pinfo("Returned result from Binary.");
    } else {
        return -1;
    }

    return 0;
}

// Handler of the RPC server
size_t server_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
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
            struct RPCProcedure rp = rpc_parse_call(payload, len);

            // Check, if we offer this procedure.
            if (rpc_check_offered(&rp) == 0) {
                pinfo("Offering desired RPC. Sending ACK.");
                // Compile and send ACK packet.
                uint8_t ack_payload[2];
                write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
                ret = msp_send(sock, ack_payload, sizeof(ack_payload));

                // Try to execute the procedure.
			    uint8_t result_payload[2 + 127 + 1];
                if (rpc_excecute(result_payload, rp) == 0) {
					pinfo("Sending result via MSP.");
        			msp_send(sock, result_payload, sizeof(result_payload));
                    pinfo("RPC execution was successful.");
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

void stopHandler (int signum) {
    pwarn("Caught signal with signum %i. Stopping RPC server.", signum);
    running = 1;
}

int rpc_listen_rhizome () {
    int return_code = 1;

    // GET RECENT FILE
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        return_code = -1;
        goto clean_rhizome_server_listener;
    }

    struct curl_slist *header = NULL;
    char *url_get = "http://localhost:4110/restful/rhizome/bundlelist.json";

    // Again, set basic options, ...
    _curl_set_basic_opt(url_get, curl_handler, header);
    // ... but this time add a callback function, where results from the cURL call are handled.
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

    // Get the bundlelist.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (get): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_server_listener_all;
    }

    {
        // Write the bundlelist to a string for manipulations.
        char json_string[(size_t)curl_result_memory.size - 1];
        memcpy(json_string, curl_result_memory.memory, (size_t)curl_result_memory.size - 1);

        // Parse JSON:
        // Init, ...
        cJSON *incoming_json = cJSON_Parse(json_string);
        // ... get the 'rows' entry, ...
        cJSON *rows = cJSON_GetObjectItem(incoming_json, "rows");
        // (if there are no files in the store, abort)
        if (cJSON_GetArraySize(rows) <= 0) {
            return_code = -1;
            goto clean_rhizome_server_listener_all;
        }
        // ... consider only the recent file, ...
        cJSON *recent_file = cJSON_GetArrayItem(rows, 0);
        // ... get the 'service', ...
        char *service = cJSON_GetArrayItem(recent_file, 2)->valuestring;
        // ... the sender from the recent file.
        char *sender = cJSON_GetArrayItem(recent_file, 11)->valuestring;
        // ... the recipient from the recent file.
        char *recipient = cJSON_GetArrayItem(recent_file, 12)->valuestring;

        // Check, if this file is an RPC packet and if it is not from but for the client.
        int service_is_rpc = strncmp(service, "RPC", strlen("RPC")) == 0;
        int not_my_file = recipient != NULL && strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid)) == 0;

        // DECRYPT IT
        if (service_is_rpc  && not_my_file) {
            // Free everyhing, again.
            _curl_reinit_memory(&curl_result_memory);
            curl_slist_free_all(header);
            header = NULL;

            // Get the bundle ID of the file which should be decrypted.
            char *bid = cJSON_GetArrayItem(recent_file, 3)->valuestring;
            char url_decrypt[117];
            sprintf(url_decrypt, "http://localhost:4110/restful/rhizome/%s/decrypted.bin", bid);

            _curl_set_basic_opt(url_decrypt, curl_handler, header);

            // Decrypt the file.
            curl_res = curl_easy_perform(curl_handler);
            if (curl_res != CURLE_OK) {
                pfatal("CURL failed (decrypt): %s.", curl_easy_strerror(curl_res));
                return_code = -1;
                goto clean_rhizome_server_listener_all;
            }

            // Copy the payload for manipulations.
            size_t filesize = cJSON_GetArrayItem(recent_file, 9)->valueint;
            uint8_t recv_payload[filesize];
            memcpy(recv_payload, curl_result_memory.memory, filesize);

            if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL) {
                pinfo("Received RPC via Rhizome.");
                // Parse the payload to the RPCProcedure struct
                struct RPCProcedure rp = rpc_parse_call(recv_payload, filesize);
        		if (str_to_sid_t(&rp.caller_sid, sender) == -1){
        			pfatal("Could not convert SID to sid_t. Aborting.");
                    return_code = -1;
                    goto clean_rhizome_server_listener_all;
        		}

                // Check, if we offer this procedure.
                if (rpc_check_offered(&rp) == 0) {
                    pinfo("Offering desired RPC. Sending ACK via Rhizome.");

                    // Compile and send ACK packet.
                    uint8_t payload[2];
                    write_uint16(&payload[0], RPC_PKT_CALL_ACK);
                    rpc_send_rhizome(rp.caller_sid, rp.name, payload);

                    // Try to execute the procedure.
				    uint8_t result_payload[2 + 127 + 1];
	                if (rpc_excecute(result_payload, rp) == 0) {
						pinfo("Sending result via Rhizome.");
        				rpc_send_rhizome(rp.caller_sid, rp.name, payload);
                        pinfo("RPC execution was successful.");
                    }
                } else {
                    pwarn("Not offering desired RPC. Ignoring.");
                    return_code = -1;
                }
            }
        }
    }
    clean_rhizome_server_listener_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_server_listener:
        _curl_free_memory(&curl_result_memory);

    return return_code;
}

static int mdp_handle(int mdp_sockfd) {
	struct mdp_header header;
	uint8_t payload[1200];

	ssize_t len = mdp_recv(mdp_sockfd, &header, payload, sizeof(payload));
	if (len == -1) {
		pwarn("Could not receive MDP payload.");
		return -1;
	}

	pdebug("Local SID: %s", alloca_tohex_sid_t(header.local.sid));

	header.local.sid = my_subscriber->sid;

	pdebug("Local SID: %s", alloca_tohex_sid_t(header.local.sid));

	if (read_uint16(&payload[0]) == RPC_PKT_CALL) {
		pinfo("Received RPC call via MDP broadcast.");
		// Parse the payload to the RPCProcedure struct
		struct RPCProcedure rp = rpc_parse_call(payload, len);

		// Check, if we offer this procedure.
		int ret = 0;
        if (rpc_check_offered(&rp) == 0) {
            pinfo("Offering desired RPC. Sending ACK.");
            // Compile and send ACK packet.
            uint8_t ack_payload[2];
            write_uint16(&ack_payload[0], RPC_PKT_CALL_ACK);
            ret = mdp_send(mdp_sockfd, &header, ack_payload, sizeof(ack_payload));

            // Try to execute the procedure.
			uint8_t result_payload[2 + 127 + 1];
			if (rpc_excecute(result_payload, rp) == 0) {
				pinfo("Sending result via MDP.");
				mdp_send(mdp_sockfd, &header, result_payload, sizeof(result_payload));
                pinfo("RPC execution was successful.");
                ret = len;
            } else {
                ret = len;
            }
        } else {
            pwarn("Not offering desired RPC. Ignoring.");
            ret = len;
        }
    }

	return 0;
}

int rpc_listen () {
    // Create address struct ...
    struct mdp_sockaddr addr_msp;
    bzero(&addr_msp, sizeof addr_msp);
    // ... and set the sid to a local sid and port where to listen at.
    addr_msp.sid = BIND_PRIMARY;
    addr_msp.port = MDP_PORT_RPC_MSP;

    // Create MDP socket.
    mdp_fd_msp = mdp_socket();

    // Sockets should not block.
    set_nonblock(mdp_fd_msp);
    set_nonblock(STDIN_FILENO);
    set_nonblock(STDERR_FILENO);
    set_nonblock(STDOUT_FILENO);

    // Create MSP socket.
    MSP_SOCKET sock_msp = msp_socket(mdp_fd_msp, 0);
    // Connect to local socker ...
    msp_set_local(sock_msp, &addr_msp);
    // ... and listen it.
    msp_listen(sock_msp);

    // Set the handler to handle incoming packets.
    msp_set_handler(sock_msp, server_handler, NULL);

    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    time_ms_t next_time;

	//
	//
	//
	//
	//
	//
	//
	int mdp_sockfd;
	if ((mdp_sockfd = mdp_socket()) < 0) {
		pfatal("Could not create MDP listening socket.");
		return -1;
	}

	struct mdp_sockaddr local_addr = {.sid = BIND_PRIMARY, .port = MDP_PORT_RPC_DISCOVER};

	if (mdp_bind(mdp_sockfd, &local_addr) < 0){
		pfatal("Could not bind to broadcast address.");
		return -1;
	}

	struct pollfd fds[2];
	fds[0].fd = mdp_sockfd;
	fds[0].events = POLLIN;



    // Listen.
    while(running < 2){
        if (running == 1) {
            sock_msp = MSP_SOCKET_NULL;
            msp_close_all(mdp_fd_msp);
            mdp_close(mdp_fd_msp);
            msp_processing(&next_time);
            break;
        }
        rpc_listen_rhizome();
        // Process MSP socket
        msp_processing(&next_time);

        msp_recv(mdp_fd_msp);

		poll(fds, 1, 500);

		if (fds->revents & POLLIN){
            mdp_handle(mdp_sockfd);
        }

        // To not drive the CPU crazy, check only once a second for new packets.
        sleep(1);

    }

    return 0;
}
