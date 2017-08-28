# ServalRPC
This documentation provides all needed information to use RPCs via Serval.

For more information and a detailed discussion about the design and the implementation for DTN-RPC, see our Paper:
*Artur Sterz, Lars Baumgärtner, Ragnar Mogk, Mira Mezini and Bernd Freisleben*, **DTN-RPC: Remote Procedure Calls for Disruption-Tolerant Networking** in IFIP Networking 2017 Conference and Workshops (Networking’2017), Stockholm, Sweden

## Building
There are three requirements, `libcurl`, `libsodium` and Serval. You can install the libs with `sudo apt install libcurl4-openssl-dev` and `sudo apt install libsodium-dev` (on other distros and OSes you have to use the respective package manager or compile the libs yourself). Serval has to be installed manually. For more information and installation instructions for Serval, see the Github page of the [Serval Project](https://github.com/servalproject/serval-dna).

For building ServalRPC run the following commands in the ServalRPC folder:

```
./make-lib.sh [<prefix>]
./configure [--prefix=<prefix>]
make
```

The first command will build a static library with all needed Serval parts. Therefore the Serval DNA will be downloaded and compiled. Then all required object files are packed to a static `.a` library. Finally everything will be cleaned up.

If your Serval instance was compiled with a `--prefix` or the `SERVAL_INSTANCE_PATH` is set, both the prefix for `make-lib.sh` and for `configure` has to be the same as the `--prefix` during the Serval compilation!

## Usage
To use ServalRPC the Serval daemon has to run.

### Server
To offer a RPC, the server has to do mainly two things: declare and implement a RPC.

#### Declaration
All offered RPCs has to be declared in a file named `rpc.conf`, which has to be located in the `$SERVAL_ETC_PATH`. It is the same folder where the general `serval.conf` file is located.

The structure is as follows: `<name> <param_count> <param_tpye_1> [<param_type_2> ...]`

Even if your RPC does not need any parameter, you have to specify at least one, which is, obviously, `void`.

The parameter types are somewhat pointless since ServalRPC does no type checks. Everything is treated as `char*`. The programmer is responsible for passing the correct parameters to the desired procedure. It is also important that the name is somewhat unique. If there are multiple procedures with the same name and the same amount of parameters, the result may be not correct if the you call this procedure not directly.

##### Example
```
add 2 int int
```

#### Implementation
After declaring a procedure, it has to be implemented. First thing to be aware of is the filename. It has to be the same as in the declaration. If the name differs, it will not be found by the RPC mechanism.

The binary has to be located under `$SERVAL_ETC_PATH/rpc_bin`.

Next it has to be an executable. So, make sure all shebangs are given, the `x`-bit is set and so on.

Also important is, that the binary should return `0` on success. All other return codes cause the server to abort execution. Negative return values are not allowed.

Since the execution is done with pipes, the result has to be `echo`'d. Whis will be parsed and sent back.

##### Example
An implementation of the declaration above could be as follows:

```
sum1="$1"
sum2="$2"
res=$(( sum1+sum2 ))
echo "$res"
exit 0
```

#### Listening
After providing a RPC, the server has to start the listeners. There are 4 listeners.

##### All
With `int rpc_server_listen ();` the server will listen on all possible connections (MSP, MDP, Rhizome). The result will be returned on the same connection where the call was received. If MSP or MDP is not possible, Rhizome will be used for this.

##### Direct
With `int rpc_server_listen_msp ();` the server will only listen for direct calls via MSP. The result will be returned via MSP, too. If this is not possible, Rhizome will be used increase the probability that the result arrives at the client.

##### Delay-tolerant
With `int rpc_server_listen_rhizome ();` the server will only listen for calls arriving via Rhizome. Both is possible, direct and broadcast. The result will only sent back via Rhizome.

##### MDP broadcast
With `int rpc_server_listen_mdp_broadcast ();` the server will only listen for calls addressed to the broadcast address via MDP. The result will be returned directly (not broadcasted) via MDP. If this is not possible, Rhizome will be used increase the probability that the result arrives at the client.

#### Implementing accpetance predicates
If your server should not accept all calls, (e.g. if you have limited energy resources) you can implement you own predicates in the function `int _rpc_server_accepts (struct RPCProcedure *rp, uint32_t requirements_raw)` in `rpc_server.c`. The implementation is only possible at compile time, so you can not add or remove predicates at runtime.
Additionally the client can send up to 8 numeric values each from 0 to 15 (4 bit). Here predefined requirements can be checked, e.g. could the first number represent some sort of priority. This is not hard defined and has to be defined globaly in the current RPC space!

The function has to return `1` if the call should be accepted and `0` otherwise.

---

The results are allways end-to-end encrypted, regardless of the chosen way.

###### Complex case
If the server receives data for complex RPCs, the data is stored in `/tmp/rpc_tmp/` with a unique filename. The first argument for that procedure is this path.

If the result of an procedure is a file, the path to that file has to be returned to the RPC server. The server will send the file back via Rhizome.

### Client
To call a RPC there are multiple ways. This could be either *directly*, if a RPC server is known. Another option is *any*. Here the call gets broadcastet. The first answer will be the result of your RPC. The third option is *all*, where the call gets broadcastet, again. But now ServalRPC will wait for up to 5 results and store them in the result array. The last mode is *transparent*, where the best solution is chosen.

#### Transparent
The *transparent* mode is called via the function `int rpc_client_call (sid_t server_sid, char *rpc_name, int paramc, char **params, uint32_t requirements)`. The `server_sid` is the SID of the server. The `rpc_name` has to be the RPC name and the `paramc` is the number of parameters needed. Finally all needed parameters has to be provides as `char **` (array of strings).

If the Server is not available via MSP the procedure will be send via Rhizome. If the `server_sid` is `any` or `broadcast` the call will be sent via MDP or Rhizome, depending on server availability in the corresponding mode.

#### Direct
The *direct* mode is triggert with `int rpc_client_call_msp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements)`. The parameters are the same as above. If the connection gets lost while waiting for the result, the result will be delivered via Rhizome. Note: In this mode the the SID can not be `SID_BROADCAST` or `SID_ANY`.

`int rpc_client_call_mdp (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements)` is also possible but more likely not successful since MDP is unreliable.

#### Any/All
With `int rpc_client_call_mdp (sid_t server_sid, char *rpc_name, int paramc, char **params, uint32_t requirements)` or `int rpc_client_call_rhizome (sid_t server_sid, char *rpc_name, int paramc, char **params, uint32_t requirements)` the call will be broadcasted via MDP. The SID can be either `SID_ANY` or `SID_BROADCAST`. The corresponding mode will be chosen (see Client introduction).

#### Delay-tolerant
With `int rpc_client_call_rhizome (sid_t sid, char *rpc_name, int paramc, char **params, uint32_t requirements)` the call will be issued via Rhizome. Here all three modes are supported (*any*, *all* or *direct*)

---

The result will be stored in a struct with the following outline:

```
struct RPCResult {
    sid_t server_sid;
    uint8_t content[RPC_PKT_SIZE];
};
```

The `sid_t server_sid` is the SID of the server which executed the call. In `content` the answer is stored. `RPC_PKT_SIZE` is the MDP MTU size minus various header sizes, usually about 1024 bytes.

All incoming answers are stored in a separate struct, which will be stored in an array of `RPCResult` structs with 5 elements.

Even if you receive less than 5 results, the array will contain data, so a simple `NULL` check is not sufficient to check if a struct contains any useable data. Therefore the the function `int rpc_client_result_get_insert_index ()` was written, which returns the number of answers in the array.

---

The `requirements` number is a 4 byte number, where each 4 bit are representing a number between 0 and 15. There predefined RPC requirements can be defined. Each number must not be greater than 15, otherwise the procedure will not be called. To get this integer, use the function `uint32_t rpc_client_prepare_requirements (int *values)`, which compiles 8 integers in an array to one `uint32_t` integer.

###### Complex case
If your RPC is more than a simple function call and you have to send files, it is only possible to send one file per call. If you need more, you have to pack them (e.g. `tar`). The path to the file which has to be sent must be the first parameter otherwise the path will be sent as a simple string. If the result is a file, too, the path to that file will be stored in `RPCResult.content`. It is also only possible to get one file back. The implementation of the RPC has to make sure that, in case, all files are packet.

## Caveats
At this point the Serval Keyring has to be unencrypted. Furthermore, the credentials for the RESTful API are __RPC__ (username) and __SRPC__ (password).
