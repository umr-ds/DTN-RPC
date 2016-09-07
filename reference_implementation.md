# ServalRPC reference implementation
## Server
### SYNOPSIS
```
servalrpc -l [-s | -d | -r]
```

### DESCRIPTION
If you build ServalRPC (see #Server), a binary called `servalrpc` will be build, too. This is a reference implemenation in C for ServalRPC.
`servalrpc -l` starts a ServalRPC listener.
Make sure to offer a RPC the right way (see #Server).

`-s, --msp` Listen only to MSP

`-d, --mdp` Listen only to MDP

`-r, --rhizome` Listen only to Rhizome

## Client
### SYNOPSIS
```
servalrpc -c [-s | -r | -d] -- (<server_sid> | (broadcast|any) | all) <procedure> <arg_1> [<arg_2> ...] <requirements>
```
### DESCRIPTION
`servalrpc -c` calls a remote procedure.

`-s, --msp` Call via MSP

`-r, --rhizome` Call via Rhizome

`-d, --mdp` Call via MDP

`(<server_sid> | (broadcast|any) | all)` Either the RPC server SID or `broadcast` or `any` or `all`. See README for modes.

`<procedure>` The name of the procedure which should be called

`<arg_1> [<arg_2> ...]` The RPC arguments.

`<requirements>` a comma-separated list of eight requirements. Example: `0,0,0,0,0,0,0,0`