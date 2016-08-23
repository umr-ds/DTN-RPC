/**** General. ****/
#define RPC_SERVER_MODE_MSP		0
#define RPC_SERVER_MODE_MDP		1
#define RPC_SERVER_MODE_RHIZOME	2
#define RPC_SERVER_MODE_ALL		3

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

int server_running;
int server_mode;

int _rpc_server_offering (struct RPCProcedure *rp);
int _rpc_server_accepts (struct RPCProcedure *rp);
int _rpc_server_excecute (uint8_t *result_payload, struct RPCProcedure rp);
struct RPCProcedure _rpc_server_parse_call (uint8_t *payload, size_t len);
void _rpc_free_rp (struct RPCProcedure rp);

/**** Rhizome part. ****/
int _rpc_server_rhizome_process ();
int _rpc_server_rhizome_send_result (sid_t sid, char *rpc_name, uint8_t *payload);

/**** MDP part. ****/
int _rpc_server_mdp_setup ();
void _rpc_server_mdp_process ();
void _rpc_server_mdp_cleanup ();

/**** MSP part. ****/
int _rpc_server_msp_setup ();
void _rpc_server_msp_process ();
void _rpc_server_msp_cleanup ();

