// General server.
int running;

int rpc_server_check_offered (struct RPCProcedure *rp);
int rpc_server_excecute (uint8_t *result_payload, struct RPCProcedure rp);
struct RPCProcedure rpc_server_parse_call (const uint8_t *payload, size_t len);

// Rhizome part.
int rpc_server_rhizome_listen ();

// MDP part.
int rpc_server_mdp_setup ();
void rpc_server_mdp_listen ();
void rpc_server_mdp_cleanup ();

// MSP part.
int rpc_server_msp_setup ();
void rpc_server_msp_listen ();
void rpc_server_msp_cleanup ();
