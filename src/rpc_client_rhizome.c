#include "rpc.h"

// Rhizome listener function.
int _rpc_client_rhizome_listen (sid_t sid, char *rpc_name) {
	int return_code = -1;

	// Init the cURL stuff.
	curl_global_init(CURL_GLOBAL_DEFAULT);
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

    char *token = NULL;
    time_t start_time = time(NULL);
    int wait_time = 20;
	while (!received) {

        //Wait for 20 seconds for answer (While develpment. Later maybe longer).
        if (time(NULL) - start_time > wait_time) {
            // If we hit the timeout, get the number of elements in the result array.
            int num_answers = rpc_client_result_get_insert_index();
            // If the SID is the broadcast id we check if we have at least one answer.
            if (is_sid_t_broadcast(sid) && num_answers > 0) {
                return_code = 2;
                received = 1;
                break;
            } else {
                return_code = -1;
                goto clean_rhizome_client_call_all;
            }
        }


        // Remove everything from the cURL memory.
	    _rpc_curl_reinit_memory(&curl_result_memory);

	    header = NULL;
        char *url_get = NULL;
        // If token is null, this is the first iteration. We then need the whole bundlelist.
        // Afterwards we only get new files since token.
        if (token) {
            url_get = calloc(64 + strlen(token), sizeof(char));
            sprintf(url_get, "http://localhost:4110/restful/rhizome/newsince/%s/bundlelist.json", token);
        } else {
            url_get = calloc(54, sizeof(char));
            sprintf(url_get, "http://localhost:4110/restful/rhizome/bundlelist.json");
        }

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
        free(url_get);

	    // Write the bundlelist to a string for manipulations.
	    char json_string[(size_t)curl_result_memory.size - 1];
	    memcpy(json_string, curl_result_memory.memory, (size_t)curl_result_memory.size - 1);

	    // Parse JSON:
	    // Init, ...
	    cJSON *incoming_json = cJSON_Parse(json_string);
	    // ... get the 'rows' entry, ...
	    cJSON *rows = cJSON_GetObjectItem(incoming_json, "rows");
        // ... get the number of files in the store since token.
        // If token is NULL, we only want the most recent file.
        int	num_files = token ? cJSON_GetArraySize(rows) : 1;

        // For every file we got
        int i;
        for (i = 0; i < num_files; i++) {
            // Get the file at position i, ...
            cJSON *recent_file = cJSON_GetArrayItem(rows, i);
            // ... get the token, ...
            token = cJSON_GetArrayItem(recent_file, 0)->valuestring;
            // ... get the 'service', ...
            char *service = cJSON_GetArrayItem(recent_file, 2)->valuestring;
            // ... get the inserttime, ...
            double inserttime = cJSON_GetArrayItem(recent_file, 6)->valuedouble;
            // ... the sender from the recent file, ...
            char *sender = cJSON_GetArrayItem(recent_file, 11)->valuestring;
            // ... the recipient from the recent file, ...
            char *recipient = cJSON_GetArrayItem(recent_file, 12)->valuestring;
            // ... the name from the recent file.
            char *name = cJSON_GetArrayItem(recent_file, 13)->valuestring;

            // Check, if this file is an RPC packet and if it is not from but for the client.
            int service_is_rpc = !strncmp(service, "RPC", strlen("RPC"));
            int not_my_file = recipient != NULL && !strcmp(recipient, alloca_tohex_sid_t(my_subscriber->sid));
            int name_is_rpc = !strncmp(name, rpc_name, strlen(rpc_name));
            int not_to_old = time(NULL) - inserttime < 600000;
            // Parse the sender SID to sid_t
            sid_t server_sid;
            str_to_sid_t(&server_sid, sender);

            if (service_is_rpc && not_my_file && name_is_rpc && not_to_old) {
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

                if (read_uint8(&recv_payload[0]) == RPC_PKT_CALL_ACK) {
                    // If this was an "all" call, and the server_sid is not in the result array yet, we store it.
                    if (is_sid_t_broadcast(sid) && _rpc_client_result_get_sid_index(sid) == -1) {
                        int position = rpc_client_result_get_insert_index();
                        memcpy(&rpc_result[position].server_sid, &server_sid, sizeof(sid_t));
                    }
                    wait_time = wait_time == 20 ? 120 : wait_time;
                    pinfo("Server %s accepted call.", sender);
                    return_code = 1;
                } else if (read_uint8(&recv_payload[0]) == RPC_PKT_CALL_RESPONSE) {
                    // If we received the result, copy it to the result array:
                    pinfo("Answer received from %s.", sender);
                    // First, see if this SID has already an entry in the result array. If not, skip.
                    int result_position = -1;
                    if (is_sid_t_broadcast(sid)) {
                        result_position = _rpc_client_result_get_sid_index(server_sid);
                        if (result_position == -1) {
                            continue;
                        }
                    }

                    // If we get an filehash, we have to doneload the file.
                    if (_rpc_str_is_filehash((char *) &recv_payload[1])) {
                        // Download the file and get the path.
                        char fpath[128 + strlen(rpc_name) + 3];
                        while (_rpc_download_file(fpath, rpc_name, alloca_tohex_sid_t(SID_BROADCAST)) != 1) sleep(1);
                        // If the result_position is -1, this was not a "all" call. So we have only one answer.
                        // We can finish.
                        if (result_position == -1) {
                            if (!is_sid_t_broadcast(sid)) {
                                memcpy(rpc_result[0].content, fpath, 128 + strlen(rpc_name) + 3);
                                memcpy(&rpc_result[0].server_sid, &server_sid, sizeof(sid_t));

                                return_code = 2;
                                received = 1;
                                break;
                            }
                        } else {
                            // Otherwise store the result at the result_position.
                            memcpy(rpc_result[result_position].content, fpath, 128 + strlen(rpc_name) + 3);
                            // If the result array is full, we do not have to wait any longer and can finish execution.
                            if (result_position == 4) {
                                return_code = 2;
                                received = 1;
                                break;
                            }
                        }
                    } else {
                        // This is the simple case. Just store the payload.
                        // If the result_position is -1, this was not a "all" call. So we have only one answer.
                        // We can finish.
                        if (result_position == -1) {
                            if (!is_sid_t_broadcast(sid)) {
                                memcpy(&rpc_result[0].content, &recv_payload[1], filesize - 1);
                                memcpy(&rpc_result[0].server_sid, &server_sid, sizeof(sid_t));
                                return_code = 2;
                                received = 1;
                                break;
                            }
                        } else {
                            // Otherwise store the result at the result_position.
                            memcpy(&rpc_result[result_position].content, &recv_payload[1], filesize - 1);
                            // If the result array is full, we do not have to wait any longer and can finish execution.
                            if (result_position == 4) {
                                return_code = 2;
                                received = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
	    sleep(1);
	}

	// Clean up.
	clean_rhizome_client_call_all:
	    curl_slist_free_all(header);
	    curl_easy_cleanup(curl_handler);
	    _rpc_curl_free_memory(&curl_result_memory);
		curl_global_cleanup();

	return return_code;
}

// Delay-tolerant call function.
int rpc_client_call_rhizome (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements) {
    // Make sure the result array is empty.
    memset(rpc_result, 0, sizeof(rpc_result));
    int return_code = -1;
	received = 0;

	// Flatten the params and replace the first parameter if it is a local path.
	char flat_params[512];
	_rpc_client_replace_if_path(flat_params, rpc_name, params, paramc);

    // Compile the call payload.
    uint8_t payload[1 + 4 + 1 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_client_prepare_call_payload(payload, paramc, rpc_name, flat_params, requirements);
    char tmp_payload_file_name[] = "/tmp/pf_XXXXXX";
    _rpc_write_tmp_file(tmp_payload_file_name, payload, sizeof(payload));

    // Construct the manifest and write it to the manifest file.
	// We have to treat it differently if the call should be braodcasted. In this case no recipient is required.
	char tmp_manifest_file_name[] = "/tmp/mf_XXXXXX";

    if (is_sid_t_broadcast(sid) || is_sid_t_any(sid)) {
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
	curl_global_init(CURL_GLOBAL_DEFAULT);
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
		curl_global_cleanup();

    return return_code;
}

