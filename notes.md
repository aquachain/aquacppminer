* --- TODO ---
* show rejected share nonce
* improve solo mining output
* logFile (mostly for solo mining)
	=> how to limit the size of this file ?
	=> define max number of lines
	=> when max reached clear out first 50% of the logFile
* pragma messages for SSE & friends (ARGON / BLAKE2B)
* pool mining fee
* solo mining fee (needs dual solo / pool mining)
* "not enough lines" issue (try solo mining with short config file to reproduce)

* GETWORK

```
010.000.002.015.44722-107.161.024.142.08888: POST 
/0x3847b2785fad1877c064cd259498ea2b5bffc01d/66 HTTP/1.1
Host: pool.aquachain-foundation.org:8888
User-Agent: Go-http-client/1.1
Content-Length: 63
Accept: application/json
Content-Type: application/json
Accept-Encoding: gzip

{"jsonrpc":"2.0","id":13,"method":"aqua_getWork","params":null}
107.161.024.142.08888-010.000.002.015.44722: HTTP/1.1 200 OK
Date: Thu, 10 May 2018 14:41:38 GMT
Content-Length: 240
Content-Type: text/plain; charset=utf-8

{"id":13,"jsonrpc":"2.0","result":["0x416e293fb0bd38d28e5ff6bd43bc13a0252333f9985485cbd5ed49f3c79d2960","0x0000000000000000000000000000000000000000000000000000000000000000","0x0431bde82d7b634dad31fcd24e160d887ebf22c01e68a0d349be8ff327aa"]}
```

* SUBMIT

```
010.000.002.015.44722-107.161.024.142.08888: POST 
/0x3847b2785fad1877c064cd259498ea2b5bffc01d/66 HTTP/1.1
Host: pool.aquachain-foundation.org:8888
User-Agent: Go-http-client/1.1
Content-Length: 222
Accept: application/json
Content-Type: application/json
Accept-Encoding: gzip

{"jsonrpc":"2.0","id":14,"method":"aqua_submitWork","params":["0x63d8903c474b670f","0x416e293fb0bd38d28e5ff6bd43bc13a0252333f9985485cbd5ed49f3c79d2960","0x0000000000000000000000000000000000000000000000000000000000000000"]}
107.161.024.142.08888-010.000.002.015.44722: HTTP/1.1 200 OK
Date: Thu, 10 May 2018 14:41:38 GMT
Content-Length: 40
Content-Type: text/plain; charset=utf-8

{"id":14,"jsonrpc":"2.0","result":true}`
```

* Sending hashrate to node ?

```
SubmitHashrate can be used for remote miners to submit their hash rate. This enables the node to report the combined
hash rate of all miners which submit work through this node. It accepts the miner hash rate and an identifier which
must be unique between nodes.
func(api *PublicMinerAPI) SubmitHashrate(hashrate hexutil.Uint64, id common.Hash) bool{
	api.agent.SubmitHashrate(id, uint64(hashrate))
	return true
}
```

