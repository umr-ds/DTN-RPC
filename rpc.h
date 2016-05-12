#include "rhizome.h"
#include "serval.h"
#include "conf.h"
#include "commandline.h"
#include "mdp_client.h"
#include "msp_client.h"
#include "dataformats.h"

#define RPC_PKT_DISCOVER 0
#define RPC_PKT_DISCOVER_ACK 1
#define RPC_PKT_CALL 2
#define RPC_PKT_CALL_ACK 3
#define RPC_PKT_CALL_RESPONSE 4

struct rpc_procedure {
    char *return_type;
    char *name;
    uint16_t paramc;
    char **params;
    sid_t caller_sid;
};

uint8_t *rpc_result[126];

// Server part.
int rpc_listen ();

// Client part.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params);

/*****************************************************************************************************************/
/* RHIZOME PART. DEPRECATED. */

struct remote_procedure {
    sid_t publisher_sid;
    const char *return_type;
    const char *name;
    int paramc;
    const char **params;
};

void rpc_write_file_rhizome (struct remote_procedure procedure, char *filepath);
int rpc_add_manifest_rhizome (sid_t publisher_sid, char *filepath, const char *rpc_name);
int rpc_publish_rhizome (sid_t publisher_sid, const char *return_type, const char *rpc_name, const char *params[]);

/* RHIZOME PART END. */
/*****************************************************************************************************************/