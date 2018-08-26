# TODO v1.3
    * testnet 
		* see testnet / testnet2 param of default miner
		* skip fees on testnet
	* -hf8 / -hf7
	* affinity, --no-affinity
	* -intensity xx
	* ARM support
		=> make sure optimization path taken

# TODO
	* AVX512F support
	* https://aquachain.github.io/pools.json
    * review what happens when pool refuses share, retry ?
	* logFile branch
	* not enough lines issue (try solo mining with short config file to reproduce)

# REJECTION EXAMPLE
	- New work info -
	hash             : 0xbf572952195aba73a07c170e283941b3af63e61862240a40b478a37a2ce56e2c
	block difficulty : 19598633046
	block target     : 5908171705931750288436467571009182098917185097320310532001048212692
	block height     : 35888

	- Latest mined block info -
	height           : 35887
	miner            : 0x9e42af44f323de092ab3e03dcef9ef6098bf4769
	diff             : 18445772279
	target           : 6277432437412353648604369448352111896592387974673237807863191392264
	nonce            : 0xb371331a487cfcaf
	version          : 2

	!!! Rejected block (nonce = 0xd14c3f50449ee0b8) !!!
	--server response:--
	{"jsonrpc":"2.0","id":183790,"result":false}


# GETWORK
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

# SUBMIT
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

# Sending hashrate to node ?
	SubmitHashrate can be used for remote miners to submit their hash rate. This enables the node to report the combined
	hash rate of all miners which submit work through this node. It accepts the miner hash rate and an identifier which
	must be unique between nodes.
	func(api *PublicMinerAPI) SubmitHashrate(hashrate hexutil.Uint64, id common.Hash) bool{
		api.agent.SubmitHashrate(id, uint64(hashrate))
		return true
	}

