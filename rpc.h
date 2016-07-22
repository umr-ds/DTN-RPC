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
#define RPC_RESET   "\033[0m"
#define RPC_FATAL   "\033[1m\033[31mRPC FATAL: \033[0m\033[31m" /* Red */
#define RPC_INFO    "\033[1m\033[32mRPC INFO: \033[0m\033[32m"  /* Green */
#define RPC_WARN    "\033[1m\033[33mRPC WARN: \033[0m\033[33m"  /* Yellow */
#define RPC_DEBUG   "\033[1m\033[34mRPC DEBUG: \033[0m\033[34m" /* Blue */

#define RPC_PKT_DISCOVER        0
#define RPC_PKT_DISCOVER_ACK    1
#define RPC_PKT_CALL            2
#define RPC_PKT_CALL_ACK        3
#define RPC_PKT_CALL_RESPONSE   4

struct RPCProcedure {
    char *return_type;
    char *name;
    uint16_t paramc;
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
