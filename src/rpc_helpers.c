#include "rpc.h"

// Function to write stuff into the corresponding log
void _rpc_eval_event (int mode, int n_args, ...) {
    // Init variadic arguments.
    va_list args;
    va_start(args, n_args);

    // Write timestamp to string
    size_t size = 13 + 1;
    char *write_buffer = calloc(size, sizeof(char));
    time_t event_time = time(NULL);
    sprintf(write_buffer, "%ld,", event_time);

    int i;
    // For every vararg: ...
    for (i = 0; i < n_args; i++) {
        // ... get the current argument, ...
        char *arg = va_arg(args, char *);
        // ... determine the new size (add 1 for ';'), ...
        size = size + strlen(arg) + 1;
        // ... make the buffer bigger ...
        write_buffer = realloc(write_buffer, size);
        // ... and store the argument in the buffer.
        strcat(write_buffer, arg);
        strcat(write_buffer, ";");
    }
    // Finally write \n and zero byte to buffer.
    size = size + 2;
    write_buffer = realloc(write_buffer, size);
    strcat(write_buffer, "\n\0");

    va_end(args);

    // Write buffer to the file.
    char eval_path[81];

    // Determine the right filename.
    if (mode == 0) {
        sprintf(eval_path, "/tmp/%s_server.csv", alloca_tohex_sid_t(my_subscriber->sid));
    } else if (mode == 1) {
        sprintf(eval_path, "/tmp/%s_client.csv", alloca_tohex_sid_t(my_subscriber->sid));
    }

    // Open file, ...
    FILE *fp = fopen(eval_path, "a+");

    // ... get the file size, ...
    size_t fs;
    fseek(fp, 0L, SEEK_END);
    fs = ftell(fp);
    rewind(fp);

    // ... write the header if it is empty ...
    if (!fs) {
        fputs("timestamp,event\n", fp);
    }

    // and finally write the content to the file.
    fputs(write_buffer, fp);

    // Cleanup
    fclose(fp);
    free(write_buffer);
}

// Write 1 byte number to payload array.
// Does not meet _rpc_.* naming convetion to keep naming similiar to other write_uint.* function from Serval.
void write_uint8 (uint8_t *payload, uint8_t value) {
	*payload = value & 0xff;
	value = value >> 8;
}

// Read 1 byte number from payload array.
// Does not meet _rpc_.* naming convetion to keep naming similiar to other write_uint.* function from Serval.
uint8_t read_uint8 (const uint8_t *payload) {
	uint8_t value = (0 << 8) | payload[0];
	return value;
}

// Prepare 2D array of params to 1D array.
char* _rpc_flatten_params (int paramc, char **params, char *delim) {
    // Determine how many chars are there.
    size_t params_size = 0;
    int i;
    for (i = 0; i < paramc; i++) {
        params_size = params_size + strlen(params[i]);
    }

    // Iterate over all params and store them in a single string, seperated with '|'
    char *flat_params  = calloc(params_size + paramc + 1, sizeof(char));
    for (i = 0; i < paramc; i++) {
        strcat(flat_params, delim);
        strcat(flat_params, params[i]);
    }

    return flat_params;
}

// Function for writing arbitary data to a temporary file. CALLER HAS TO REMOVE IT!
size_t _rpc_write_tmp_file (char *file_name, void *content, size_t len) {
    // Create a filedescriptor and open the file.
    int tmp_file_fd = mkstemp(file_name);
    FILE *tmp_file = fdopen(tmp_file_fd, "wb+");
    // Write conten.
    size_t written_size = fwrite(content, 1, len, tmp_file);
    // Close, cleanup.
    fclose(tmp_file);
    return written_size;
}

// Function for adding a file to the Rhizome store for complex RPC part.
int _rpc_add_file_to_store (char *filehash, sid_t sid, char *rpc_name, char *filepath) {
	int result = 0;
    // Construct the manifest and write it to the manifest file. We have to treat it differently if the call is braodcasted.
	char tmp_manifest_file_name[] = "/tmp/mf_XXXXXX";
	if (is_sid_t_broadcast(sid)) {
		int manifest_size = strlen("service=RPC\nname=f_\nsender=\n") + strlen(rpc_name) + strlen(alloca_tohex_sid_t(sid));
		char manifest_str[manifest_size];
		sprintf(manifest_str, "service=RPC\nname=f_%s\nsender=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid));
		_rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));
	} else {
		int manifest_size = strlen("service=RPC\nnamef_=\nsender=\nrecipient=\n") + strlen(rpc_name) + (strlen(alloca_tohex_sid_t(sid)) * 2);
	    char manifest_str[manifest_size];
	    sprintf(manifest_str, "service=RPC\nname=f_%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));
	    _rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));
	}

    // Init the cURL stuff.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _rpc_curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        result = -1;
        goto clean_rhizome_insert;
    }

    // Declare all needed headers forms and URLs.
    struct curl_slist *header = NULL;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    char *url_insert = "http://localhost:4110/restful/rhizome/insert";

    // Set basic cURL options (see function).
    _rpc_curl_set_basic_opt(url_insert, curl_handler, header);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _rpc_curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

    // Add the manifest and payload form and add the form to the cURL request.
    _rpc_curl_add_file_form(tmp_manifest_file_name, filepath, curl_handler, formpost, lastptr);

    // Perfom request, which means insert the RPC file to the store.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (post add file): %s. Aborting.", curl_easy_strerror(curl_res));
        result = -1;
        goto clean_rhizome_insert_all;
    }

	// Search for the last '=' and skip it to get the filehash.
	char *response_filehash = &strrchr((char *) curl_result_memory.memory, '=')[1];
	// Store the filehash in the variable and make sure it is \0 terminated.
	memcpy(filehash, response_filehash, 128);
	memcpy(&filehash[128], "\0", 1);

    // Clean up.
	clean_rhizome_insert_all:
		curl_slist_free_all(header);
    clean_rhizome_insert:
		curl_easy_cleanup(curl_handler);
		_rpc_curl_free_memory(&curl_result_memory);
		remove(tmp_manifest_file_name);
        curl_global_cleanup();

    return result;
}

// Function to check if a SID is reachable via MDP or MSP.
int _rpc_sid_is_reachable (sid_t sid) {
	// Create a MDP socket.
	int mdp_sockfd;
	if ((mdp_sockfd = mdp_socket()) < 0) {
		pfatal("Could not create MDP lookup socket. Aborting.");
		return -1;
	}

	// Init a mdp_frame
	overlay_mdp_frame mdp;
	bzero(&mdp,sizeof(mdp));

	// Set the packettype to get a routingtable.
	mdp.packetTypeAndFlags = MDP_ROUTING_TABLE;
	overlay_mdp_send(mdp_sockfd, &mdp, 0, 0);

	// Poll until there is nothing left.
	while (overlay_mdp_client_poll(mdp_sockfd, 200)) {
		// Create mdp_frame for incoming data. TTL is required but not needed...
		overlay_mdp_frame recv_frame;
		int ttl;
		if (overlay_mdp_recv(mdp_sockfd, &recv_frame, 0, &ttl)) {
      		continue;
  		}

		// Handle incoming data.
		int offset=0;
		while (offset + sizeof(struct overlay_route_record) <= recv_frame.out.payload_length) {
			// Make new route record struct where all needed information is stored in.
			struct overlay_route_record *record = &recv_frame.out.route_record;
			offset += sizeof(struct overlay_route_record);

			// If the record (aka SID) is reachable, and the record is the desired SID, return 1 and clean up.
			if ((record->reachable == REACHABLE_INDIRECT || record->reachable == REACHABLE_UNICAST || record->reachable == REACHABLE_BROADCAST || record->reachable == REACHABLE_SELF)
					&& !cmp_sid_t(&record->sid, &sid)) {
				mdp_close(mdp_sockfd);
				return 1;
			}
		}
	}
  mdp_close(mdp_sockfd);
  return 0;
}

// Download file if it is an complex RPC.
int _rpc_download_file (char *fpath, char *rpc_name, char *sid) {
	sid_t event_sid;
	str_to_sid_t(&event_sid, sid);
    int return_code = -1;

	// Init the cURL stuff.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _rpc_curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        return_code = -1;
        goto clean_rhizome_server_listener;
    }

	// Declare all needed headers forms and URLs.
    struct curl_slist *header = NULL;
    char *url_get = "http://localhost:4110/restful/rhizome/bundlelist.json";

    // Set basic cURL options and a callback function, where results from the cURL call are handled.
    _rpc_curl_set_basic_opt(url_get, curl_handler, header);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _rpc_curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

    // Get the bundlelist.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (get server download file): %s. Aborting.", curl_easy_strerror(curl_res));
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
		int num_rows = cJSON_GetArraySize(rows);
        if (num_rows <= 0) {
            return_code = 0;
            goto clean_rhizome_server_listener_all;
        }

		int i;
		for (i = 0; i < num_rows; i++) {
	        // ... consider only the recent file, ...
	        cJSON *recent_file = cJSON_GetArrayItem(rows, i);
	        // ... get the 'service', ...
	        char *service = cJSON_GetArrayItem(recent_file, 2)->valuestring;
			// ... the insert time.
	        int64_t in_time = (int64_t) cJSON_GetArrayItem(recent_file, 4)->valuedouble;
	        // ... the sender from the recent file.
	        char *sender = cJSON_GetArrayItem(recent_file, 11)->valuestring;
	        // ... the recipient from the recent file.
	        char *recipient = cJSON_GetArrayItem(recent_file, 12)->valuestring;
	        // ... the recipient from the recent file.
	        char *name = cJSON_GetArrayItem(recent_file, 13)->valuestring;

			char rpc_cmp_name[4 + strlen(rpc_name)];
			sprintf(rpc_cmp_name, "f_%s", rpc_name);

	        // Check, if this file is an RPC packet and if it is not from but for the client.
	        int service_is_rpc = !strncmp(service, "RPC", strlen("RPC"));
	        int filename_is_right = !strncmp(name, rpc_cmp_name, strlen(rpc_cmp_name));
            int right_client = sender && (!strncmp(alloca_tohex_sid_t(SID_BROADCAST), sid, strlen(sid)) || !strncmp(sender, sid, strlen(sid)));
			int not_my_file = 0;
			if (recipient) {
	        	not_my_file = recipient != NULL && !strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid));
			} else {
				not_my_file = 1;
			}

	        // If this is an interesting file: handle it.
	        if (service_is_rpc  && not_my_file && filename_is_right && right_client) {
	            // Free everyhing, again.

				if ((curl_handler = curl_easy_init()) == NULL) {
			        pfatal("Failed to create curl handle in post. Aborting.");
			        return_code = -1;
			        goto clean_rhizome_server_listener;
			    }

	            _rpc_curl_reinit_memory(&curl_result_memory);
	            curl_slist_free_all(header);
	            header = NULL;

				char rpc_down_name[4 + strlen(rpc_name) + sizeof(in_time) + sizeof(sender)];
				sprintf(rpc_down_name, "f_%s_%" PRId64 "_%s", rpc_name, in_time, sender);

				char filepath[strlen(RPC_TMP_FOLDER) + strlen(rpc_down_name)];
				sprintf(filepath, "%s%s", RPC_TMP_FOLDER, rpc_down_name);
				mkdir(RPC_TMP_FOLDER, 0700);
				FILE* rpc_file = fopen(filepath, "w");

	            // Get the bundle ID of the file which should be decrypted.
	            char *bid = cJSON_GetArrayItem(recent_file, 3)->valuestring;
	            char url_decrypt[117];
	            sprintf(url_decrypt, "http://localhost:4110/restful/rhizome/%s/decrypted.bin", bid);

	            _rpc_curl_set_basic_opt(url_decrypt, curl_handler, header);
        		curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _rpc_curl_write_to_file);
				curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, rpc_file);

	            // Decrypt the file.
	            curl_res = curl_easy_perform(curl_handler);
	            if (curl_res != CURLE_OK) {
	                pfatal("CURL failed (decrypt server download file): %s.", curl_easy_strerror(curl_res));
	                return_code = -1;
	                goto clean_rhizome_server_listener_all;
	            }
				fclose(rpc_file);
				strcpy(fpath, filepath);
				return_code = 1;
				break;
	        }
	    }
	}
    clean_rhizome_server_listener_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_server_listener:
        _rpc_curl_free_memory(&curl_result_memory);
        curl_global_cleanup();

    return return_code;
}

// Function to check if a string is a filehash.
// Very basic. Check, if it has 128 hex chars.
int _rpc_str_is_filehash (char *hash) {
	int len_ok = strlen(hash) == 128;
	int is_hex = !hash[strspn(hash, "0123456789ABCDEF")];
	return len_ok && is_hex;
}

// Function to free a procedure struct.
void _rpc_free_rp (struct RPCProcedure rp) {
    free(rp.name);
    free(rp.paramc.paramc_s);
    int i;
    for (i = 0; i < rp.paramc.paramc_n; i++) {
        free(rp.params[i]);
    }
    //free(rp.params);
}
