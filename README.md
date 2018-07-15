# Aquacppminer
[Aquachain](https://aquachain.github.io/) C++ optimized miner.

### Binaries
Download [latest binaries](https://bitbucket.org/cryptogone/aquacppminer/downloads)

    aquacppminer     : slowest version, should work on any CPU
    aquacppminer_avx : faster version, for CPUs supporting AVX instruction set
    aquacppminer_avx2: fastest version, for recent CPUs supporting AVX2 instruction set

Use a tool like CPUZ to see if your CPU supports AVX / AVX2

### Installation
On Linux you need to install required packages first

    ubuntu : sudo apt-get install libssl-dev libcurl3
    redhat : sudo yum install libcurl-devel gmp-devel openssl-devel

Other platforms, just unzip to a folder and launch

### Config file
* First time you launch the miner it will ask for configuration and store it into config.cfg. 
* You can edit this file later if you want, delete config.cfg and relaunch the miner to restart configuration
* If using commandline parameters (see next section) miner will not create config file.
* Commandline parameters have priority over config file.

### Usage
    aquacppminer -F url [-t nThreads] [-n nodeUrl] [--solo] [-r refreshRate] [-h]
        -F url        : url of pool or node to mine on, if not specified, will pool mine to dev's aquabase
        -t nThreads   : number of threads to use (if not specified will use maximum logical threads available)
        -n node_url   : optional node url, to get more stats (pool mining only)
        -r rate       : pool refresh rate, ex: 3s, 2.5m, default is 3s
        --solo        : solo mining, -F needs to be the node url
        -h            : display this help message and exit

### Examples

Pool Mining, auto thread count

    aquacppminer -F http://pool.aquachain-foundation.org:8888/0x6e37abb108f4010530beb4bbfd9842127d8bfb3f

Pool Mining, 8 threads, getting more block stats from local aqua node

    aquacppminer -t 8 -F http://pool.aquachain-foundation.org:8888/0x6e37abb108f4010530beb4bbfd9842127d8bfb3f -n http://127.0.0.1:8543

Solo Mining to local aqua node, auto thread count

    aquacppminer --solo -F http://127.0.0.1:8543

### Fee
Miner takes a 3% fee, applies to solo & pool mining.

### Donations
* AQUA : 0x6e37abb108f4010530beb4bbfd9842127d8bfb3f

### Social
* Email: cryptogone.dev@gmail.com
* Twitter: [@CryptoGonus](https://twitter.com/CryptoGonus)
* Discord: cryptogone#3107
