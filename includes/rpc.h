#include <curl/curl.h>
#include <stddef.h>
#include <signal.h>

#include "conf.h"
#include "dataformats.h"
#include "mdp_client.h"
#include "msp_client.h"
#include "serval.h"
#include "server.h"

#include "rpc_server.h"
#include "rpc_client.h"
#include "rpc_helpers.h"
#include "cJSON.h"

/**** General. ****/
// MDP ports
#define MDP_PORT_RPC		18
#define MDP_PORT_RPC_MSP	112

// String defines
#define RPC_CONF_FILENAME	"rpc.conf"
#define RPC_TMP_FOLDER		"/tmp/rpc_tmp/"
#define SERVAL_FOLDER		"/serval/"
#define BIN_FOLDER			"/serval/rpc_bin/"

// Packet types
#define RPC_PKT_CALL            0
#define RPC_PKT_CALL_CHUNK      1
#define RPC_PKT_CALL_ACK        2
#define RPC_PKT_CALL_RESPONSE   3

/**** Server part. ****/
// Transparent
int rpc_server_listen ();
// Direct
int rpc_server_listen_msp ();
// Delay-tolerant
int rpc_server_listen_rhizome ();
// MDP
int rpc_server_listen_mdp_broadcast ();

/**** Client part. ****/
int received;
uint8_t *rpc_result[MDP_MTU];

// Transparent
int rpc_client_call (sid_t server_sid, char *rpc_name, int paramc, char **params);
// Direct
int rpc_client_call_msp (sid_t sid, char *rpc_name, int paramc, char **params);
// Delay-tolerant (any/direct)
int rpc_client_call_rhizome (sid_t sid, char *rpc_name, int paramc, char **params);
// Any
int rpc_client_call_mdp (sid_t server_sid, char *rpc_name, int paramc, char **params);
