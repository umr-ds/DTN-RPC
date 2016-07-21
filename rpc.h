#include "rhizome.h"
#include "serval.h"
#include "server.h"
#include "conf.h"
#include "commandline.h"
#include "mdp_client.h"
#include "msp_client.h"
#include "dataformats.h"
#include "cJSON.h"
#include <curl/curl.h>

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

// Server part.
int rpc_listen ();

// Client part.
int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params);

// cURL helpers.
struct CurlResultMemory {
  char *memory;
  size_t size;
};

size_t _curl_write_response (void *contents, size_t size, size_t nmemb, void *userp);

void _curl_init_memory (struct CurlResultMemory *curl_result_memory);
void _curl_reinit_memory (struct CurlResultMemory *curl_result_memory);
void _curl_free_memory (struct CurlResultMemory *curl_result_memory);

void _curl_set_basic_opt (char* url, CURL *curl_handler, struct curl_slist *header);

void _curl_add_file_form (char *tmp_manifest_file_name, char *tmp_payload_file_name, CURL *curl_handler, struct curl_httppost *formpost, struct curl_httppost *lastptr);

// General helpers.
char* _rpc_flatten_params (const int paramc, const char **params, const char *delim);
uint8_t *_rpc_prepare_call_payload (uint8_t *payload, const int paramc, const char *rpc_name, const char *flat_params);
size_t _rpc_write_tmp_file (char *file_name, void *content, size_t len);
