#include "rpc.h"

// Rhizome listener function.
int _rpc_client_rhizome_listen (sid_t sid, char *rpc_name) {
	int return_code = -1;

	// Init the cURL stuff.
	CURL *curl_handler = NULL;
	CURLcode curl_res;
	struct CurlResultMemory curl_result_memory;
	struct curl_slist *header = NULL;
	_rpc_curl_init_memory(&curl_result_memory);
	if ((curl_handler = curl_easy_init()) == NULL) {
	    pfatal("Failed to create curl handle in get. Aborting.");
	    return_code = -1;
	    goto clean_rhizome_client_call_all;
	}

	while (received == 0) {
	    // Remove everything from the cURL memory.
	    _rpc_curl_reinit_memory(&curl_result_memory);

	    header = NULL;
	    char *url_get = "http://localhost:4110/restful/rhizome/bundlelist.json";

	    // Again, set basic options, ...
	    _rpc_curl_set_basic_opt(url_get, curl_handler, header);
	    // ... but this time add a callback function, where results from the cURL call are handled.
	    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _rpc_curl_write_response);
	    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

	    // Get the bundlelist.
	    curl_res = curl_easy_perform(curl_handler);
	    if (curl_res != CURLE_OK) {
	        pfatal("CURL failed (get Rhizome call): %s. Aborting.", curl_easy_strerror(curl_res));
	        return_code = -1;
	        goto clean_rhizome_client_call_all;
	    }

	    // Write the bundlelist to a string for manipulations.
	    char json_string[(size_t)curl_result_memory.size - 1];
	    memcpy(json_string, curl_result_memory.memory, (size_t)curl_result_memory.size - 1);

	    // Parse JSON:
	    // Init, ...
	    cJSON *incoming_json = cJSON_Parse(json_string);
	    // ... get the 'rows' entry, ...
	    cJSON *rows = cJSON_GetObjectItem(incoming_json, "rows");
	    // ... consider only the recent file, ...
	    cJSON *recent_file = cJSON_GetArrayItem(rows, 0);
	    // ... get the 'service', ...
	    char *service = cJSON_GetArrayItem(recent_file, 2)->valuestring;
	    // ... the recipient from the recent file.
	    char *recipient = cJSON_GetArrayItem(recent_file, 12)->valuestring;

	    // Check, if this file is an RPC packet and if it is not from but for the client.
	    int service_is_rpc = strncmp(service, "RPC", strlen("RPC")) == 0;
	    int not_my_file = recipient != NULL && strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid)) == 0;

	    if (service_is_rpc  && not_my_file) {
	        // Free everyhing, again.
	        _rpc_curl_reinit_memory(&curl_result_memory);
	        curl_slist_free_all(header);
	        header = NULL;

	        // Get the bundle ID of the file which should be decrypted.
	        char *bid = cJSON_GetArrayItem(recent_file, 3)->valuestring;
	        char url_decrypt[117];
	        sprintf(url_decrypt, "http://localhost:4110/restful/rhizome/%s/decrypted.bin", bid);

	        _rpc_curl_set_basic_opt(url_decrypt, curl_handler, header);

	        // Decrypt the file.
	        curl_res = curl_easy_perform(curl_handler);
	        if (curl_res != CURLE_OK) {
	            pfatal("CURL failed (decrypt Rhizome call): %s.", curl_easy_strerror(curl_res));
	            return_code = -1;
	            goto clean_rhizome_client_call_all;
	        }

	        // Copy the payload for manipulations.
	        size_t filesize = cJSON_GetArrayItem(recent_file, 9)->valueint;
	        uint8_t recv_payload[filesize];
	        memcpy(recv_payload, curl_result_memory.memory, filesize);

	        if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL_ACK) {
	            // If we got an ACK packet, we wait (for now) 5 more seconds.
	            pinfo("Received ACK via Rhizome. Waiting.");
				return_code = 1;
	        } else if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL_RESPONSE) {
	            // We got our result!
	            pinfo("Received result.");
	            memcpy(rpc_result, &recv_payload[2], filesize - 2);
				if (_rpc_str_is_filehash((char *) rpc_result)) {
					char fpath[128 + strlen(rpc_name) + 3];
					while (_rpc_download_file(fpath, rpc_name, alloca_tohex_sid_t(sid)) != 0) sleep(1);
					memcpy(rpc_result, fpath, 128 + strlen(rpc_name) + 3);
				}
	            return_code = 2;
	            received = 1;
	            break;
	        }
	    }
	    sleep(1);
	}

	// Clean up.
	clean_rhizome_client_call_all:
	    curl_slist_free_all(header);
	    curl_easy_cleanup(curl_handler);
	    _rpc_curl_free_memory(&curl_result_memory);

	return return_code;
}

// Delay-tolerant call function.
int rpc_client_call_rhizome (sid_t sid, char *rpc_name, int paramc, char **params) {
	// Set the client_mode to non-transparent if it is not set yet, but leaf it as is otherwise.
	client_mode = client_mode == RPC_CLIENT_MODE_TRANSPARTEN ? RPC_CLIENT_MODE_TRANSPARTEN : RPC_CLIENT_MODE_NON_TRANSPARENT;
    int return_code = -1;
	received = 0;

	// Flatten the params.
	char *flat_params;
	if (strncmp(params[0], "filehash", strlen("filehash")) == 0 && paramc >= 2) {
		// Add the file to the Rhizome store given as the second parameter and replace the local path with the filehash.
		char filehash[129];
		_rpc_add_file_to_store(filehash, SID_BROADCAST, rpc_name, params[1]);

		char *new_params[paramc];
		new_params[0] = (char *) params[0];
		new_params[1] = filehash;

		int i;
		for (i = 2; i < paramc; i++) {
			new_params[i] = (char *) params[i];
		}

		flat_params = _rpc_flatten_params(paramc, (char **) new_params, "|");
	} else {
		flat_params = _rpc_flatten_params(paramc, params, "|");
	}

    // Construct the payload and write it to the payload file.
    // |------------------------|-------------------|----------------------------|--------------------------|
    // |-- 2 byte packet type --|-- 2 byte paramc --|-- strlen(rpc_name) bytes --|-- strlen(params) bytes --|
    // |------------------------|-------------------|----------------------------|--------------------------|
    // One extra byte for string termination.
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, flat_params);
    char tmp_payload_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_payload_file_name, payload, sizeof(payload));

    // Construct the manifest and write it to the manifest file.
	// We have to treat it differently if the call should be braodcasted. In this case no recipient is required.
	char tmp_manifest_file_name[L_tmpnam];
	if (is_sid_t_broadcast(sid)) {
		int manifest_size = strlen("service=RPC\nname=\nsender=\n") + strlen(rpc_name) + strlen(alloca_tohex_sid_t(sid));
		char manifest_str[manifest_size];
		sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid));
		_rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));
	} else {
		int manifest_size = strlen("service=RPC\nname=\nsender=\nrecipient=\n") + strlen(rpc_name) + (strlen(alloca_tohex_sid_t(sid)) * 2);
	    char manifest_str[manifest_size];
	    sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));
	    _rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));
	}


    // Init the cURL stuff.
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _rpc_curl_init_memory(&curl_result_memory);
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in post. Aborting.");
        return_code = -1;
        goto clean_rhizome_client_call;
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
    _rpc_curl_add_file_form(tmp_manifest_file_name, tmp_payload_file_name, curl_handler, formpost, lastptr);

    // Perfom request, which means insert the RPC file to the store.
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (post Rhizome call): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_client_call_all;
    }

	// Listen until we receive something.
	return_code = _rpc_client_rhizome_listen(sid, rpc_name);

    // Clean up.
    clean_rhizome_client_call_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_client_call:
        _rpc_curl_free_memory(&curl_result_memory);
        remove(tmp_manifest_file_name);
        remove(tmp_payload_file_name);

    return return_code;
}
