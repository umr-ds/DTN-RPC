#include <curl/curl.h>
#include <stddef.h>

#include "conf.h"
#include "dataformats.h"
#include "mdp_client.h"
#include "msp_client.h"
#include "serval.h"
#include "server.h"

// General.
#define MDP_PORT_RPC_DISCOVER	18
#define MDP_PORT_RPC_MSP		112

#define RPC_CONF_FILENAME "rpc.conf"
#define SERVAL_FOLDER "/serval/"
#define BIN_FOLDER "/serval/rpc_bin/"

#define RPC_PKT_CALL            0
#define RPC_PKT_CALL_ACK        1
#define RPC_PKT_CALL_RESPONSE   2

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

#include "rpc_server.h"
#include "rpc_helpers.h"
#include "cJSON.h"

/**** Server part. ****/
// At this point only one main listener.
int rpc_listen ();

/**** Client part. ****/
int received;
uint8_t *rpc_result[126];

// Transparent
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params);
// Direct
int rpc_client_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params);
// Delay-tolerant (any/direct)
int rpc_client_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params);
// Any
int rpc_client_call_mdp_broadcast (const char *rpc_name, const int paramc, const char **params);
