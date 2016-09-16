#include "rpc.h"

// Send the result via Rhizome
int _rpc_server_rhizome_send_result (sid_t sid, char *rpc_name, uint8_t *payload) {
    int return_code = 1;

    _rpc_eval_event(0, 3, "preparing result Rhizome", alloca_tohex_sid_t(sid), rpc_name);
    // Construct the manifest and write it to the manifest file.
    int manifest_size = strlen("service=RPC\nname=\nsender=\nrecipient=\n") + strlen(rpc_name) + (strlen(alloca_tohex_sid_t(sid)) * 2);
    char manifest_str[manifest_size];
    sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));
    char tmp_manifest_file_name[] = "/tmp/mf_XXXXXX";
    _rpc_write_tmp_file(tmp_manifest_file_name, manifest_str, strlen(manifest_str));

    // Write the payload to the payload file.
    char tmp_payload_file_name[] = "/tmp/pf_XXXXXX";
    _rpc_write_tmp_file(tmp_payload_file_name, payload, RPC_PKT_SIZE-1);

    // Init the cURL stuff.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl_handler = NULL;
    CURLcode curl_res;
    struct CurlResultMemory curl_result_memory;
    _rpc_curl_init_memory(&curl_result_memory);
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

    // Set basic cURL options and a callback function, where results from the cURL call are handled.
    _rpc_curl_set_basic_opt(url_insert, curl_handler, header);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, _rpc_curl_write_response);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, (void *) &curl_result_memory);

    // Add the manifest and payload form and add the form to the cURL request.
    _rpc_curl_add_file_form(tmp_manifest_file_name, tmp_payload_file_name, curl_handler, formpost, lastptr);

    // Perfom request, which means insert the RPC file to the store.
    _rpc_eval_event(0, 3, "inserting result Rhizome", alloca_tohex_sid_t(sid), rpc_name);
    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        pfatal("CURL failed (post server result): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_server_response_all;
    }

    // Clean up.
    clean_rhizome_server_response_all:
        curl_slist_free_all(header);
        curl_easy_cleanup(curl_handler);
    clean_rhizome_server_response:
        _rpc_curl_free_memory(&curl_result_memory);
        remove(tmp_manifest_file_name);
        remove(tmp_payload_file_name);
        curl_global_cleanup();

    return return_code;
}

// Rhizome server processing.
int _rpc_server_rhizome_process () {
    int return_code = 0;

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
        pfatal("CURL failed (get server process): %s. Aborting.", curl_easy_strerror(curl_res));
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
            return_code = 1;
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
        // Parse the sender SID to sid_t
        sid_t server_sid;
        str_to_sid_t(&server_sid, sender);

        // Check, if this file is an RPC packet and if it is not from but for the client.
        int service_is_rpc = !strncmp(service, "RPC", strlen("RPC"));
		int not_my_file = 0;
		if (recipient) {
        	not_my_file = recipient != NULL && !strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid));
		} else {
			not_my_file = 1;
		}

        // If this is an interesting file: handle it.
        if (service_is_rpc  && not_my_file) {
            _rpc_eval_event(0, 2, "Found potential call Rhizome", sender);
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
            _rpc_eval_event(0, 2, "extracting potential call Rhizome", sender);
            curl_res = curl_easy_perform(curl_handler);
            if (curl_res != CURLE_OK) {
                pfatal("CURL failed (decrypt server process): %s.", curl_easy_strerror(curl_res));
                return_code = -1;
                goto clean_rhizome_server_listener_all;
            }

            // Copy the payload for manipulations.
            size_t filesize = cJSON_GetArrayItem(recent_file, 9)->valueint;
            uint8_t recv_payload[filesize];
            memcpy(recv_payload, curl_result_memory.memory, filesize);

            if (read_uint8(&recv_payload[0]) == RPC_PKT_CALL) {
                _rpc_eval_event(0, 2, "call received Rhizome", sender);
                pinfo("Received RPC via Rhizome.");
                // Parse the payload to the RPCProcedure struct
                struct RPCProcedure rp = _rpc_server_parse_call(recv_payload, filesize);
        		if (str_to_sid_t(&rp.caller_sid, sender) == -1) {
        			pfatal("Could not convert SID to sid_t. Aborting.");
                    return_code = -1;
                    goto clean_rhizome_server_listener_all;
        		}

                // Check, if we offer this procedure and we should accept the call.
                if (_rpc_server_offering(&rp) && _rpc_server_accepts(&rp, read_uint32(&recv_payload[1]))) {
                    _rpc_eval_event(0, 2, "sending ACK Rhizome", sender);
                    pinfo("Offering desired RPC. Sending ACK via Rhizome.");

                    // Compile and send ACK packet.
                    uint8_t payload[1];
                    write_uint8(&payload[0], RPC_PKT_CALL_ACK);
                    _rpc_server_rhizome_send_result(rp.caller_sid, rp.name, payload);

                    // Try to execute the procedure.
				    uint8_t result_payload[RPC_PKT_SIZE];
                    memset(result_payload, 0, RPC_PKT_SIZE);
	                if (_rpc_server_excecute(result_payload, rp)) {
                        _rpc_eval_event(0, 2, "sending result Rhizome", sender);
						pinfo("Sending result via Rhizome.");
        				_rpc_server_rhizome_send_result(rp.caller_sid, rp.name, result_payload);
                        _rpc_eval_event(0, 2, "RPC success Rhizome", sender);
                        pinfo("RPC execution was successful.\n");
                    }
                } else {
                    pwarn("Not offering desired RPC. Ignoring.");
                    return_code = -1;
                }
                _rpc_free_rp(rp);
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
