#include "rpc.h"

// The RPC cliend handler
size_t _rpc_client_msp_handler (MSP_SOCKET sock, msp_state_t state, const uint8_t *payload, size_t len, void *UNUSED(context)) {
    size_t ret = 0;

    // If there is an errer on the socket, stop it.
    if (state & (MSP_STATE_CLOSED | MSP_STATE_ERROR)) {
        pwarn("Socket closed.");
        received = received == 1 || received == 2 ? received : -1;
        msp_stop(sock);
    }

    // If the other site closed the connection, we do also.
    if (state & MSP_STATE_SHUTDOWN_REMOTE) {
        pwarn("Socket shutdown.");
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
            pinfo("Server accepted call.");
            received = 1;
        } else if (pkt_type == RPC_PKT_CALL_RESPONSE) {
            pinfo("Answer received.");
            memcpy(rpc_result, &payload[2], len - 2);
            received = 2;
        }
    }
    return ret;
}

// Direct call function.
int rpc_client_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params) {
    // Create address struct ...
    struct mdp_sockaddr addr;
    bzero(&addr, sizeof addr);
    // ... and set the sid and port of the server.
    addr.sid = sid;
    addr.port = MDP_PORT_RPC_MSP;

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
    msp_set_handler(sock, _rpc_client_msp_handler, NULL);

    char *flat_params = _rpc_flatten_params(paramc, params, "|");

    // Construct the payload and write it to the payload file.
    // |------------------------|-------------------|----------------------------|--------------------------|
    // |-- 2 byte packet type --|-- 2 byte paramc --|-- strlen(rpc_name) bytes --|-- strlen(params) bytes --|
    // |------------------------|-------------------|----------------------------|--------------------------|
    // 1 extra byte for string termination.
    uint8_t payload[2 + 2 + strlen(rpc_name) + strlen(flat_params) + 1];
    _rpc_prepare_call_payload(payload, paramc, rpc_name, flat_params);

    pinfo("Calling %s for %s.", alloca_tohex_sid_t(sid), rpc_name);

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
