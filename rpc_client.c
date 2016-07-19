#include "rpc.h"

int received = 0;

struct data {
  char trace_ascii; /* 1 or 0 */
};

static
void dump_curl(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex)
{
  size_t i;
  size_t c;

  unsigned int width=0x10;

  if(nohex)
    /* without the hex output, we can fit more on screen */
    width = 0x40;

  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);

  for(i=0; i<size; i+= width) {

    fprintf(stream, "%4.4lx: ", (long)i);

    if(!nohex) {
      /* hex not disabled, show it */
      for(c = 0; c < width; c++)
        if(i+c < size)
          fprintf(stream, "%02x ", ptr[i+c]);
        else
          fputs("   ", stream);
    }

    for(c = 0; (c < width) && (i+c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */
      if(nohex && (i+c+1 < size) && ptr[i+c]==0x0D && ptr[i+c+1]==0x0A) {
        i+=(c+2-width);
        break;
      }
      fprintf(stream, "%c",
              (ptr[i+c]>=0x20) && (ptr[i+c]<0x80)?ptr[i+c]:'.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */
      if(nohex && (i+c+2 < size) && ptr[i+c+1]==0x0D && ptr[i+c+2]==0x0A) {
        i+=(c+3-width);
        break;
      }
    }
    fputc('\n', stream); /* newline */
  }
  fflush(stream);
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  struct data *config = (struct data *)userp;
  const char *text;
  (void)handle; /* prevent compiler warning */

  switch (type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }

  dump_curl(text, stderr, (unsigned char *)data, size, config->trace_ascii);
  return 0;
}

// NOT NEEDED SO FAR! WILL BE NEEDED FOR BROADCASTS.
//int rpc_discover (sid_t server_sid, char *rpc_name) {
//    int mdp_sockfd;
//    if ((mdp_sockfd = mdp_socket()) < 0) {
//        return WHY("Cannot create MDP socket");
//    }
//
//    struct mdp_header mdp_header;
//    bzero(&mdp_header, sizeof(mdp_header));
//
//    mdp_header.local.sid = BIND_PRIMARY;
//    mdp_header.remote.sid = server_sid;
//    mdp_header.remote.port = MDP_PORT_RPC_DISCOVER;
//    mdp_header.qos = OQ_MESH_MANAGEMENT;
//    mdp_header.ttl = PAYLOAD_TTL_DEFAULT;
//
//    if (mdp_bind(mdp_sockfd, &mdp_header.local) < 0){
//        printf("RPC WARN: COULD NOT BIND\n");
//        return -1;
//    }
//
//    time_ms_t finish = gettime_ms() + 10;
//    int acked = 0;
//
//    while (gettime_ms() < finish && acked != 1) {
//        uint8_t payload[strlen(rpc_name) + 2];
//        write_uint16(&payload[0], RPC_PKT_DISCOVER);
//        memcpy(&payload[2], rpc_name, strlen(rpc_name));
//
//        if (mdp_send(mdp_sockfd, &mdp_header, payload, sizeof(payload)) < 0){
//            printf("RPC DEBUG: SEND FAIL\n");
//            return -1;
//        }
//        if (mdp_poll(mdp_sockfd, 500)<=0)
//            continue;
//
//        struct mdp_header mdp_recv_header;
//        uint8_t recv_payload[strlen(rpc_name)];
//        ssize_t len = mdp_recv(mdp_sockfd, &mdp_recv_header, recv_payload, sizeof(recv_payload));
//
//        if (len < 0) {
//            printf("RPC DEBUG: LEN FAIL\n");
//            break;
//        }
//
//        if (read_uint16(&recv_payload[0]) == RPC_PKT_DISCOVER_ACK) {
//            acked = 1;
//            printf("RPC DEBUG: RECEIVED: %d, LEN: %i\n", read_uint16(&recv_payload[0]), len);
//        }
//
//
//    }
//    return acked;
//}

// The RPC cliend handler
size_t client_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    size_t ret = 0;

    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        printf("RPC WARN: Socket closed.\n");
        received = received == 1 || received == 2 ? received : -1;
        msp_stop(sock);
    }

    // If the other site closed the connection, we do also.
    if (state & MSP_STATE_SHUTDOWN_REMOTE) {
        printf("RPC WARN: Socket shutdown\n");
        received = received == 1 || received == 2 ? received : -1;
        msp_shutdown(sock);
    }

    // If we have payload handle it.
    if (payload && len) {
        ret = len;
        // Get the packet type.
        uint16_t pkt_type = read_uint16(&payload[0]);
        // If we receive an ACK, just print.
        if (pkt_type == RPC_PKT_CALL_ACK) {
            printf("RPC DEBUG: Server accepted call.\n");
            received = 1;
        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
            printf("RPC DEBUG: Answer received.\n");
            memcpy(rpc_result, &payload[2], len - 2);
            received = 2;
        }
    }
    return ret;
}

// Direct call function.
int rpc_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    // Create address struct ...
    struct mdp_sockaddr addr;
    bzero(&addr, sizeof addr);
    // ... and set the sid and port of the server.
    addr.sid = sid;
    addr.port = 112;

    // Create MDP socket.
    int mdp_fd = mdp_socket();

    // Sockets should not block.
    set_nonblock(mdp_fd);
    set_nonblock(STDIN_FILENO);
    set_nonblock(STDOUT_FILENO);

    // Create a poll struct, where the polled packets are handled.
    struct pollfd fds[2];
    fds->fd = mdp_fd;
    fds->events = POLLIN | POLLERR;

    // Create MSP socket.
    MSP_SOCKET sock = msp_socket(mdp_fd, 0);
    // Connect to the server.
    msp_connect(sock, &addr);

    // Set the handler to handle incoming packets.
    msp_set_handler(sock, client_handler, NULL);

    // Prepare 2D array of params to 1D for serialization. Use '|' as a seperator.
    char *flat_params  = malloc(sizeof(char *));
    sprintf(flat_params, "|%s", params[0]);
    int i;
    for (i = 1; i < paramc; i++) {
        strcat(flat_params, "|");
        strncat(flat_params, params[i], strlen(params[i]));
    }

    // Create the payload containing ...
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params)];
    // ... the packet type, ...
    write_uint16(&payload[0], RPC_PKT_CALL);
    // ... number of parameters, ...
    write_uint16(&payload[2], (uint16_t) paramc);
    // ... the rpc name ...
    memcpy(&payload[4], rpc_name, strlen(rpc_name));
    // ... and all params.
    memcpy(&payload[4 + strlen(rpc_name)], flat_params, strlen(flat_params));

    printf("RPC DEBUG: Calling %s for %s.\n", alloca_tohex_sid_t(sid), rpc_name);

    // Send the payload.
    msp_send(sock, payload, sizeof(payload));

    time_t timeout = time(NULL);

    // While we have not received the answer...
    while(received == 0 || received == 1){
        // Process MSP socket
        time_ms_t next_time;
        msp_processing(&next_time);
        time_ms_t poll_timeout = next_time - gettime_ms();

        // We only wait for 3 seconds.
        if ((double) (time(NULL) - timeout) >= 3.0) {
            break;
        }

        // Poll the socket
        poll(fds, 1, poll_timeout);

        // If something arrived, receive it.
        if (fds->revents & POLLIN){
            msp_recv(mdp_fd);
        }
    }

    // Clean up.
    sock = MSP_SOCKET_NULL;
    msp_close_all(mdp_fd);
    mdp_close(mdp_fd);

    return received;
}

char* rpc_flatten_params (const int paramc, const char **params) {
    // Prepare 2D array of params to 1D for serialization. Use '|' as a seperator.
    size_t params_size = 0;
    int i;
    for (i = 0; i < paramc; i++){
        params_size = params_size + strlen(params[i]);
    }

    char *flat_params  = malloc(params_size + paramc);
    sprintf(flat_params, "|%s", params[0]);
    for (i = 1; i < paramc; i++) {
        strcat(flat_params, "|");
        strncat(flat_params, params[i], strlen(params[i]));
    }

    return flat_params;
}

int rpc_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    int return_code = -1;

    struct data config;
    config.trace_ascii = 1;

    if (!(keyring = keyring_open_instance(""))){
        printf("RPC WARN: Could not open keyring file. Aborting.\n");
        return -1;
    }
    keyring_enter_pin(keyring, "");

    char *payload_flat_params = rpc_flatten_params(paramc, params);

    size_t payload_size = 2 + 2 + strlen(rpc_name) + strlen(payload_flat_params);
    uint8_t payload[payload_size];
    write_uint16(&payload[0], RPC_PKT_CALL);
    write_uint16(&payload[2], (uint16_t) paramc);
    memcpy(&payload[4], rpc_name, strlen(rpc_name));
    memcpy(&payload[4 + strlen(rpc_name)], payload_flat_params, strlen(payload_flat_params) + 1);

    char tmp_payload_file_name[L_tmpnam];
    tmpnam(tmp_payload_file_name);
    FILE *tmp_payload_file = fopen(tmp_payload_file_name, "w+");
    fwrite(payload, sizeof(payload), sizeof(payload), tmp_payload_file);
    fclose(tmp_payload_file);

    char manifest_str[512];
    sprintf(manifest_str, "service=RPC\nname=%s\nsender=%s\nrecipient=%s\n", rpc_name, alloca_tohex_sid_t(my_subscriber->sid), alloca_tohex_sid_t(sid));

    char tmp_manifest_file_name[L_tmpnam];
    tmpnam(tmp_manifest_file_name);
    FILE *tmp_manifest_file = fopen(tmp_manifest_file_name, "w+");
    fputs(manifest_str, tmp_manifest_file);
    fclose(tmp_manifest_file);

    CURL *curl_handler;
    CURLcode curl_res;
    if ((curl_handler = curl_easy_init()) == NULL) {
        printf("RPC WARN: Failed to create curl handle in fetch_session. Aborting.");
        return -1;
    }

    curl_easy_setopt(curl_handler, CURLOPT_DEBUGFUNCTION, my_trace);
    curl_easy_setopt(curl_handler, CURLOPT_DEBUGDATA, &config);
    curl_easy_setopt(curl_handler, CURLOPT_VERBOSE, 1L);

    struct curl_slist *header = NULL;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    char *url = "http://localhost:4110/restful/rhizome/insert";

    curl_easy_setopt(curl_handler, CURLOPT_URL, url);
    curl_easy_setopt(curl_handler, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl_handler, CURLOPT_USERNAME, "RPC");
    curl_easy_setopt(curl_handler, CURLOPT_PASSWORD, "SRPC");


    header = curl_slist_append(header, "Expect:");
    curl_easy_setopt(curl_handler, CURLOPT_HTTPHEADER, header);

    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "manifest", CURLFORM_FILE, tmp_manifest_file_name, CURLFORM_CONTENTTYPE, "rhizome/manifest; format=text+binarysig", CURLFORM_END);

    CURLFORMcode c = curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "payload", CURLFORM_FILE, tmp_payload_file_name, CURLFORM_CONTENTTYPE, "application/octet-stream", CURLFORM_END);

    printf("\nRPC DEBUG: #### C (payload) = %d\n", c);

    curl_easy_setopt(curl_handler, CURLOPT_HTTPPOST, formpost);

    curl_res = curl_easy_perform(curl_handler);
    if (curl_res != CURLE_OK) {
        printf("RPC WARN: CURL failed: %s\n", curl_easy_strerror(curl_res));
    }

    remove(tmp_manifest_file_name);
    remove(tmp_payload_file_name);
    curl_slist_free_all(header);
    curl_easy_cleanup(curl_handler);

    return return_code = -1;
    //
    //
    // // Define the cursor, which iterates through the database.
    // struct rhizome_list_cursor cursor;
    // // For some reason, we need to zero out the memory, where the pointer to the cursor points to.
    // bzero(&cursor, sizeof(cursor));
    // // We set the cursor service to RPC, to skip MeshMS and files.
    // cursor.service = "RPC";
    // // Now we open the cursor.
    // if (rhizome_list_open(&cursor) == -1) {
    //     keyring_free(keyring);
    //     keyring = NULL;
    //     return -1;
    // }
    //
    // time_t timeout = time(NULL);
    // while (received == 0) {
    //     // We only wait for 3 seconds (while developing. Lateron definitely longer).
    //     if ((double) (time(NULL) - timeout) >= 3.0) {
    //         break;
    //     }
    //
    //     // Define a rhizome_read struct, where the reading of the content happens.
    //     struct rhizome_read read_state;
    //     // Again, we have to zero it out.
    //     bzero(&read_state, sizeof(read_state));
    //     printf("RPC DEBUG: Waiting...\n");
    //
    //     // Iterate through the rhizome store.
    //     while (rhizome_list_next(&cursor) == 1) {
    //         // Get the current manifest.
    //         rhizome_manifest *m = cursor.manifest;
    //
    //         // Open the file and prepare to read it.
    //         enum rhizome_payload_status status = rhizome_open_decrypt_read(m, &read_state);
    //         // If the file with the given hash is in the rhizome store, we read is.
    //         if (status == RHIZOME_PAYLOAD_STATUS_STORED) {
    //             // Create a buffer with the size if the file, where the content get written to.
    //             unsigned char buffer[m->filesize];
    //             // Read the content of the file, while its not empty.
    //             while((rhizome_read(&read_state, buffer, sizeof(buffer))) > 0){
    //                 if (read_uint16(&buffer[0]) == RPC_PKT_CALL_ACK){
    //                     printf("RPC DEBUG: Received ACK via Rhizome. Waiting.\n");
    //                     timeout = time(NULL);
    //                 } else if (read_uint16(&buffer[0]) == RPC_PKT_CALL_RESPONSE) {
    //                     printf("RPC DEBUG: Received result.\n");
    //                     memcpy(rpc_result, &buffer[2], m->filesize - 2);
    //                     return_code = 2;
    //                     received = 1;
    //                     break;
    //                 }
    //             }
    //             if (return_code == 2) {
    //                 break;
    //             }
    //         }
    //     }
    //     sleep(1);
    // }
    //
    // // Now we can clean up and free everything.
    // rhizome_list_release(&cursor);
    // keyring_free(keyring);
    // keyring = NULL;
    //
    // return return_code;
}

// General call function. For transparent usage.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params) {
    // TODO: THIS IS FOR ANY/ALL RPCS.
    if (is_sid_t_any(server_sid) || is_sid_t_broadcast(server_sid)) {
        return -1;
    } else {
        // Call the rpc directly over msp.
        int call_return = call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);//rpc_call_msp(server_sid, rpc_name, paramc, params);

        if (call_return == -1) {
            printf("RPC WARN: Server not available via MSP. Trying Rhizome.\n");
            //call_return = rpc_call_rhizome(server_sid, rpc_name, paramc, params);
            return -1;
        } else if (call_return == 0) {
            printf("RPC DEBUG: NOT RECEIVED ACK\n");
            return -1;
        } else if (call_return == 1) {
            printf("RPC DEBUG: COULD NOT COLLECT RESULT AFTER ACK\n");
            return -1;
        } else {
            return 0;
        }
    }

    return 1;
}
