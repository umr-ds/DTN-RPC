#define RPC_RESET   "\033[0m"
#define RPC_FATAL   "\033[1m\033[31mRPC FATAL: \033[0m\033[31m" /* Red */
#define RPC_INFO    "\033[1m\033[32mRPC INFO: \033[0m\033[32m"  /* Green */
#define RPC_WARN    "\033[1m\033[33mRPC WARN: \033[0m\033[33m"  /* Yellow */
#define RPC_DEBUG   "\033[1m\033[34mRPC DEBUG: \033[0m\033[34m" /* Blue */

#define pfatal(fmt, ...)    printf(RPC_FATAL fmt RPC_RESET "\n", ##__VA_ARGS__);
#define pinfo(fmt, ...)     printf(RPC_INFO fmt RPC_RESET "\n", ##__VA_ARGS__);
#define pwarn(fmt, ...)     printf(RPC_WARN fmt RPC_RESET "\n", ##__VA_ARGS__);
#define pdebug(fmt, ...)    printf(RPC_DEBUG fmt RPC_RESET "\n", ##__VA_ARGS__);

// cURL helpers.
struct CurlResultMemory {
  char *memory;
  size_t size;
};

size_t _rpc_curl_write_response (void *contents, size_t size, size_t nmemb, void *userp);
size_t _rpc_curl_write_to_file (void *contents, size_t size, size_t nmemb, FILE *dst_file);

void _rpc_curl_init_memory (struct CurlResultMemory *curl_result_memory);
void _rpc_curl_reinit_memory (struct CurlResultMemory *curl_result_memory);
void _rpc_curl_free_memory (struct CurlResultMemory *curl_result_memory);

void _rpc_curl_set_basic_opt (char* url, CURL *curl_handler, struct curl_slist *header);

void _rpc_curl_add_file_form (const char *tmp_manifest_file_name, const char *tmp_payload_file_name, CURL *curl_handler, struct curl_httppost *formpost, struct curl_httppost *lastptr);

// General helpers.
char* _rpc_flatten_params (const int paramc, const char **params, const char *delim);
uint8_t *_rpc_prepare_call_payload (uint8_t *payload, const int paramc, const char *rpc_name, const char *flat_params);
size_t _rpc_write_tmp_file (char *file_name, void *content, size_t len);
int _rpc_add_file_to_store (char *filehash, const sid_t sid, const char *rpc_name, const char *filepath);
int _rpc_server_rhizome_download_file (char *fpath, const char *rpc_name, char *client_sid);
