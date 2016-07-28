#include <curl/curl.h>
#include <stddef.h>

#include "rpc_helpers.h"
#include "cJSON.h"

#include "conf.h"
#include "dataformats.h"
#include "mdp_client.h"
#include "msp_client.h"
#include "serval.h"
#include "server.h"


// General.
#define RPC_PKT_DISCOVER        0
#define RPC_PKT_DISCOVER_ACK    1
#define RPC_PKT_CALL            2
#define RPC_PKT_CALL_ACK        3
#define RPC_PKT_CALL_RESPONSE   4

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

uint8_t *rpc_result[126];

/**** Server part. ****/
// At this point only one main listener.
int rpc_listen ();

/**** Client part. ****/
// Transparent function.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params);
// Direct function.
int rpc_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params);
// Delay-tolerant function.
int rpc_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params);
