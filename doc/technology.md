# ServalRPC technology
## API
### Server
#### Transparent
```
int rpc_server_listen ();
```
This call starts the server listening on all three channels, MSP, MDP and Rhizome. The result will be sent back via the same channel the call was received. If this is not possible (if for example a MSP connection gets lost) the result will be sent back via Rhizome.

#### Direct
```
int rpc_server_listen_msp ();
```
This call starts the server listening for incoming MSP connections addressed to this particular server.

#### MDP
```
int rpc_server_listen_mdp_broadcast ();
```
This call starts the server listening for incoming MDP connections. This can be either addressed to this server or broadcasted calls.

#### Delay-tolerant
```
int rpc_server_listen_rhizome ();
```
This call starts the server listening for incoming Rhizome files. This can be either addressed to this server or broadcasted calls.

### Client
#### Transparent
```
int rpc_client_call (sid_t server_sid, char *rpc_name, int paramc, char **params, uint32_t requirements);
```
Calls the procedure whereas the way is chosen transparently.
##### Parameters
* `server_sid` The SID of the server which should execute the call.
	* Can be a SID, SID\_BROADCAST or SID\_ANY
* `rpc_name` The name of the remote procedure
* `paramc` Number of parameters
* `params` Parameter string array
* `requirements` Requirements.

##### Returns
Returns `2` on success, `-1` on failure, `0` if a connection to the server could be established but nothing else and `1` if ACK was received but no result.

If the call was with a SID or SID\_ANY, the result is in `rpc_result[0]`, if the call was with `SID_BROADCAST` the results are in the `rpc_result` array.

#### Direct
```
int rpc_client_call_msp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements);
int rpc_client_call_mdp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements);
```
Calls the procedure either via MDP or MSP. MSP may take milliseconds longer but MDP is not reliable.
##### Parameters
* `server_sid` The SID of the server which should execute the call.
	* Can NOT be SID\_BROADCAST or SID\_ANY
* `rpc_name` The name of the remote procedure
* `paramc` Number of parameters
* `params` Parameter string array
* `requirements` Requirements.

##### Returns
Returns `2` on success, `-1` on failure, `0` if a connection to the server could be established but nothing else and `1` if ACK was received but no result.

The result is in `rpc_result[0]`.

#### Any/All
```
int rpc_client_call_mdp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements);
int rpc_client_call_rhizome (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements);
```
Calls the procedure either via MDP or delay-tolerant via Rhizome. If the server is not available you have to chose Rhizome. None of the both ways guarantee anything.
##### Parameters
* `server_sid` The SID of the server which should execute the call.
	* Can be SID\_BROADCAST or SID\_ANY
* `rpc_name` The name of the remote procedure
* `paramc` Number of parameters
* `params` Parameter string array
* `requirements` Requirements.

##### Returns
Returns `2` on success, `-1` on failure, `0` if a connection to the server could be established but nothing else and `1` if ACK was received but no result.

If the call was with SID\_ANY, the result is in `rpc_result[0]`, if the call was with `SID_BROADCAST` the results are in the `rpc_result` array.

#### Other functions
```
int rpc_client_result_get_insert_index ();
```
Returns the number of results in the result array.

```
uint32_t rpc_client_prepare_requirements (int *values);
```
Stores the `values` in an integer of type `uint32_t`. This is needed for the RPC calls (`requirements` parameter)

##### Parameters
* `values` Integer array with exactly 8 numbers, each between 0 and 15, inclusively.

##### Returns
Returns an integer where all eight numbers are stored with 4 bits each.

#### Others
```
struct RPCResult {
    sid_t server_sid;
    uint8_t content[RPC_PKT_SIZE];
};
```
Result struct. `server_sid` is the SID of the server which executed the call. `content` is the actual result.

```
struct RPCResult rpc_result[5];
```
Array which can hold up to five result structs.

## Packet types
ServalRPC uses 3 different packets.

##### Explanations

* `TYPE`: 1 byte
	* Indicates the packet type.
	* `0`: Call, `1`: ACK, `2` Result
* `REQUIREMENTS`: 4 bytes
	* Freely definable requirements for RPC
*  `#PARAMETERS`: 1 byte
	* Number of parameters for the call.
	
### Call
The call packets have a maximum size of `RPC_PKT_SIZE` which is dynamically calculated based on servals `MDP_MTU`.
The call consists of the header and the payload, both in the same packet.
The packet type is `0`

```
HEADER:
 0               1               2               3               4               5
 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     TYPE    |                          REQUIREMENTS                         |  #PARAMETERS  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The payload can hold up to `RPC_PKT_SIZE - 5 - 1` bytes of data. 5 bytes are needed for the header and 1 byte is needed for the `\0` byte at the end to make the parsing on server side easier.

### ACK
The ACK packet has a size of exactly one byte for the packet type.
The packet type is `1`

```
HEADER:
 0              
 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+
 |     TYPE    |
 +-+-+-+-+-+-+-+
```

### Result
The result packets have a maximum size of `RPC_PKT_SIZE` which is dynamically calculated based on servals `MDP_MTU`.
The call consists of the header and the payload, both in the same packet.
The packet type is `2`

```
HEADER:
 0              
 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+
 |     TYPE    |
 +-+-+-+-+-+-+-+
```
The payload can hold up to `RPC_PKT_SIZE - 1 - 1` bytes of data. 1 byte is needed for the header and 1 byte is needed for the `\0` byte at the end to make the parsing on client side easier.