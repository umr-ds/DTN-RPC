#include "rpc.h"

// Prepare 2D array of params to 1D array.
char* _rpc_flatten_params (const int paramc, const char **params, const char *delim) {
    // Determine how many chars are there.
    size_t params_size = 0;
    int i;
    for (i = 0; i < paramc; i++) {
        params_size = params_size + strlen(params[i]);
    }

    // Iterate over all params and store them in a single string, seperated with '|'
    char *flat_params  = calloc(params_size + paramc, sizeof(char));
    for (i = 0; i < paramc; i++) {
        strcat(flat_params, delim);
        strncat(flat_params, params[i], strlen(params[i]));
    }

    return flat_params;
}

// Prepare the payload whith the RPC information.
uint8_t *_rpc_prepare_call_payload (uint8_t *payload, const int paramc, const char *rpc_name, const char *flat_params) {
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

// Function for writing arbitary data to a temporary file. CALLER HAS TO REMOVE IT!
size_t _rpc_write_tmp_file (char *file_name, void *content, size_t len) {
    // Create tmp file.
    tmpnam(file_name);
    // Open the file.
    FILE *tmp_file = fopen(file_name, "wb+");
    // Write the data.
    size_t written_size = fwrite(content, 1, len, tmp_file);
    // Close the file.
    fclose(tmp_file);
    return written_size;
}

// Function for adding a file to the Rhizome store for complex RPC part.
int _rpc_add_file_to_store (char *filehash, const sid_t sid, const char *rpc_name, const char *filepath) {
	int result = 0;
    // Construct the manifest and write it to the manifest file. We have to treat it differently if the call is braodcasted.
	char tmp_manifest_file_name[L_tmpnam];
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
			if ((record->reachable == REACHABLE || record->reachable == REACHABLE_SELF)
					&& cmp_sid_t(&record->sid, &sid) == 0) {
				mdp_close(mdp_sockfd);
				return 1;
			}
		}
	}
  mdp_close(mdp_sockfd);
  return 0;
}
