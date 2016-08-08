#include "rpc.h"

int rpc_client_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    int return_code = -1;
	received = 0;

    // Flatten the params.
    char *payload_flat_params = _rpc_flatten_params(paramc, params, "|");

    // Construct the payload and write it to the payload file.
    // |------------------------|-------------------|----------------------------|--------------------------|
    // |-- 2 byte packet type --|-- 2 byte paramc --|-- strlen(rpc_name) bytes --|-- strlen(params) bytes --|
    // |------------------------|-------------------|----------------------------|--------------------------|
    // One extra byte for string termination.
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(payload_flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, payload_flat_params);
    char tmp_payload_file_name[L_tmpnam];
    _rpc_write_tmp_file(tmp_payload_file_name, payload, sizeof(payload));

    // Construct the manifest and write it to the manifest file.
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
        pfatal("CURL failed (post): %s. Aborting.", curl_easy_strerror(curl_res));
        return_code = -1;
        goto clean_rhizome_client_call_all;
    }

    /* LISTENER PART (waiting for response) */
    // Reinit to forget about the post part above.
    if ((curl_handler = curl_easy_init()) == NULL) {
        pfatal("Failed to create curl handle in get. Aborting.");
        return_code = -1;
        goto clean_rhizome_client_call_all;
    }

    // We only wait for a few seconds (while developing. Lateron definitely longer).
    time_t timeout = time(NULL);
    int waittime = 10;
    while (received == 0) {
        if ((double) (time(NULL) - timeout) >= waittime) {
            break;
        }

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
            pfatal("CURL failed (get): %s. Aborting.", curl_easy_strerror(curl_res));
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
                pfatal("CURL failed (decrypt): %s.", curl_easy_strerror(curl_res));
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
                waittime = 20;
            } else if (read_uint16(&recv_payload[0]) == RPC_PKT_CALL_RESPONSE) {
                // We got our result!
                pinfo("Received result.");
                memcpy(rpc_result, &recv_payload[2], filesize - 2);
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
    clean_rhizome_client_call:
        _rpc_curl_free_memory(&curl_result_memory);
        remove(tmp_manifest_file_name);
        remove(tmp_payload_file_name);

        return return_code;
}
