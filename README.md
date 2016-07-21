# Serval RPC documentation

This documentation provides all needed information to use RPCs via Serval.

## Server
To offer a RPC, the server has to do mainly two things: declare and implement a RPC.

### Declaration
All offered RPCs has to be declared in a file named `rpc.conf`, which has to be located in the `$SERVAL_ETC_PATH`. It is the same folder where the general `serval.conf` file is located.

The structure is as follows: `<return_type> <name> <param_tpye_1> [<param_type_2> ...]`

Even if your RPC does not need any parameter, you have to specify at least one, which is, obviously, `void`. Same for the return type.

#### Example
```
int add int int
```

### Implementation
After declaring a procedure, it has to be implemented. First thing to be aware of is the filename. It has to be the same as in the declaration. If the name differs, it will not be found by the RPC mechanism.

The binary has to be located under `$SERVAL_ETC_PATH/rpc_bin`.

Next it has to be an executable. So, make sure all shebangs are given, the `x`-bit is set and so on.

Also important is, that the binary should return either `0` on succes or `1` otherwise. All other return codes cause the server to abort execution.

Since the execution is done with pipes, the result has to be `echo`\'d. Whis will be parsed and sent back.

#### Example
An implementation of the declaration above could be as follows:

```
sum1="$1"
sum2="$2"
res=$(( sum1+sum2 ))
echo "$res"
exit 0
```
__*TODO*: FILEHASH (KOMPLEX)__

## Client
To call a RPC there are multiple ways. This could be either *directly*, if a RPC server is known or *any*. The third mode is *transparent*, where the best solution is chosen.

### Transparent
The *transparent* mode is called via the function `int rpc_call (const sid_t server_sid, const char *rpc_name, const int paramc, const char **params)`. The `server_sid` is the SID of the server. If the SID is not known, it should be `SID_ANY` or `SID_BROADCAST` (note: if the SID is not known, the mode will be allways *any*). The `rpc_name` has to be the RPC name and the `paramc` is the number of parameters needed. Finally all needed parameters has to be provides as `char **` (Stringarray).

If the Server is not available via MSP the procedure will be send via Rhizome.

The result will be always encrypted, regardless of the mode.

### Direct
The direct mode is triggert with `int rpc_call_msp (const sid_t sid, const char *rpc_name, const int paramc, const char **params)`. The parameters are the same as above. Note: if the server is not available, the RPC will not be executed. But if the connections gets lost while waiting for the result, the result will be delivered via Rhizome.

__*TODO*: DIRECT HOP COUNT; FILEHASH (KOMPLEX); WHERE IS THE RESULT?; OTHER MODES__

## Caveats
At this point the Serval Keyring has to be unencrypted. Furthermore, the credentials for the RESTful API are __RPC__ (username) and __SRPC__ (password).
