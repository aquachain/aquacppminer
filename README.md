# Aquacppminer
[Aquachain](https://aquachain.github.io/) C++ optimized miner.

# Binaries
    Use a tool like CPUZ to see if your CPU supports AVX / AVX2
      aquacppminer      : slowest version, should work on any 64bit CPU
      aquacppminer_avx  : faster version, for CPUs supporting AVX instruction set
      aquacppminer_avx2 : fastest version, for recent CPUs supporting AVX2 instruction set

# Config file
    First time you launch the miner it will ask for configuration and store it into config.cfg.
    You can edit this file later if needed, delete it and relaunch the miner to recreate a new one.
    If using commandline parameters (see next section) miner will not create config file.
    Commandline parameters have priority over config file.

# Usage
    aquacppminer.exe -F url [-t nThreads] [-n nodeUrl] [--solo] [-r refreshRate] [-h]
        -F url        : url of pool or node to mine on, if not specified, will pool mine to dev's aquabase
        -t nThreads   : number of threads to use (if not specified will use maximum logical threads available)
        -n node_url   : optional node url, to get more stats (pool mining only)
        -r rate       : pool refresh rate, ex: 1s, 2.5m, default is 1s
        --solo        : solo mining, -F needs to be the node url
        -h            : display this help message and exit

# Examples
    Pool Mining, auto thread count
        aquacppminer -F http://pool.aquachain-foundation.org:8888/0x6e37abb108f4010530beb4bbfd9842127d8bfb3f
    Pool Mining, 8 threads, getting more block stats from aqua node 167.99.139.123
        aquacppminer -F http://pool.aquachain-foundation.org:8888/0x6e37abb108f4010530beb4bbfd9842127d8bfb3f -n http://167.99.139.123:8543
    Solo Mining to aqua node 167.99.139.123, auto thread count
        aquacppminer --solo -F http://167.99.139.123:8543

# Fee
    Miner takes 2% fee. This applies to solo & pool mining.

# Donations
    AQUA : 0x6e37abb108f4010530beb4bbfd9842127d8bfb3f
