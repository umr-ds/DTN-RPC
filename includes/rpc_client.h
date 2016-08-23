/**** General part. ****/

// Functions
int _rpc_client_replace_if_path (char *flat_params, char *rpc_name, char **params, int paramc);
uint8_t *_rpc_client_prepare_call_payload (uint8_t *payload, int paramc, char *rpc_name, char *flat_params);

/**** Rhizome part. ****/
int _rpc_client_rhizome_listen ();

