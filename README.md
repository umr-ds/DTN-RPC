# ServalRPC API documentation
This documentation provides all needed information to use RPCs via Serval.

## Building
There is only one requirement, `libcurl`. On most systems this is already installed.

For building ServalRPC run the following commands in the ServalRPC folder:

```
./make-lib.sh [<prefix>]
./configure [--prefix=<prefix>]
make
```

The first command will build a static library with all needed Serval parts. Therefore the Serval DNA will be downloaded and compiled. Then all required object files are packed to a static `.a` library. Finally everything will be cleaned up.

If your Serval instance was compiled with a `--prefix`, both the prefix for `make-lib.sh` and for `configure` has to be the same!

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
If your server should not accept all calls, (e.g. if you have limited energy resources) you can implement you own predicates in the function `int _rpc_server_accepts (struct RPCProcedure *UNUSED(rp))` in `rpc_server.c`. The implementation is only possible at compile time, so you can not add or remove predicates at runtime.

The function has to return `1` if the call should be accepted and `0` otherwise.

---

The results are allways end-to-end encrypted, regardless of the chosen way.

###### Complex case
If the server receives data for complex RPCs, the data is stored in `/tmp/rpc_tmp/` with a unique filename. The first argument for that procedure is this path.

If the result of an procedure is a file, the path to that file has to be returned to the RPC server. The server will send the file back via Rhizome.

### Client
To call a RPC there are multiple ways. This could be either *directly*, if a RPC server is known or *any*. The third mode is *transparent*, where the best solution is chosen.

#### Transparent
The *transparent* mode is called via the function `int rpc_client_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params)`. The `server_sid` is the SID of the server. The `rpc_name` has to be the RPC name and the `paramc` is the number of parameters needed. Finally all needed parameters has to be provides as `char **` (array of strings).

If the Server is not available via MSP the procedure will be send via Rhizome.

The result will be always encrypted, regardless of the mode.

#### Direct
The *direct* mode is triggert with `int rpc_client_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params)`. The parameters are the same as above. Note: if the server is not available, the RPC will not be executed. But if the connections gets lost while waiting for the result, the result will be delivered via Rhizome.

#### Any
With `int rpc_client_call_mdp_broadcast (const char *rpc_name, const int paramc, const char **params);` the call will be broadcasted via MDP.

#### Delay-tolerant
With `int rpc_client_call_rhizome (const sid_t sid, const char *rpc_name, const int paramc, const char **params);` the call will be issued via Rhizome. If you want to broadcast the call via Rhizome, set the `sid` to `SID_BROADCAST`.

---

The result will be stored in `rpc_result`, which is an `\0` terminated array of type `uint8_t`.

###### Complex case
If your RPC is more than a simple function call and you have to send files, it is only possible to send one file per call. If you need more, you have to pack them (e.g. `tar`). The path to the file which has to be sent must be the first parameter otherwise the path will be sent as a simple string. If the result is a file, too, the path to that file will be stored in `rpc_result`. It is also only possible to get one file back. The implementation of the RPC has to make sure that, in case, all files are packet.

## Caveats
At this point the Serval Keyring has to be unencrypted. Furthermore, the credentials for the RESTful API are __RPC__ (username) and __SRPC__ (password).
