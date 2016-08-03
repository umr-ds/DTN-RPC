// General server.
struct ParamRepr {
	uint16_t paramc_n;
	char *paramc_s;
};

struct RPCProcedure {
    char *name;
    struct ParamRepr paramc;
    char **params;
    sid_t caller_sid;
};

int running;


int _rpc_server_check_offered (struct RPCProcedure *rp);
int _rpc_server_excecute (uint8_t *result_payload, struct RPCProcedure rp);
struct RPCProcedure _rpc_server_parse_call (const uint8_t *payload, size_t len);

// Rhizome part.
int _rpc_server_rhizome_process ();

// MDP part.
int _rpc_server_mdp_setup ();
void _rpc_server_mdp_process ();
void _rpc_server_mdp_cleanup ();

// MSP part.
int _rpc_server_msp_setup ();
void _rpc_server_msp_process ();
void _rpc_server_msp_cleanup ();
