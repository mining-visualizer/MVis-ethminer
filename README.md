## MVis-ethminer


This page describes the miner for **0xBitcoin**.  For the regular ethereum miner, [click here](https://github.com/mining-visualizer/MVis-ethminer).

This is a fork of my MVis-ethminer program, which was a fork of Genoil's ethminer-0.9.41-genoil-1.x.x. 

* This miner is for AMD gpu's only.
* Source code is on the **0xbitcoin** branch. The version number series for binaries is 2.x
* The **Master** branch is for Ethereum mining.  The version number series is 1.x
* Windows binaries can be downloaded from the  [Releases](https://github.com/mining-visualizer/MVis-ethminer/releases) page, or you can build from source (see below).
* Linux support is limited.  The original MVis-ethminer worked well on Linux, so I see no reason why this shouldn't as well, but I haven't tried building or running it under Linux.  You can try building from source and see if it works.
* Currently only solo mining is supported.  That means you either need to run your own node, or use a public one,  like Infura.
* Pool mining support will hopefully be added soon.


### Installation

* Unzip the [download package](https://github.com/mining-visualizer/MVis-ethminer/releases) anywhere you like.  
* Move `ethminer.ini` to `%LocalAppData%/ethminer` on Windows, or `$HOME/.config/ethminer` on Linux.  If that folder path does not exist, you will need to create it manually.
* Various settings need to be specified in the `.ini` file.  They are fairly well commented.  Have a look at them.
    * Input an Ethereum account and associated private key.  Yah, I know ... that sucks!  A PK in plain text format on your hard drive!  Make sure it is a 'throw away' account with only the bare minimum amount of money.  I might get around to implementing a proper, secure vault someday, although if someone wants to submit a PR, I'd be happy to merge it in.
    * You can specify the address of your node in the `.ini` file, or on the command line.
    * You can enable gas price bidding.  (see comments in the file).  Note that enabling this feature does not guarantee that you will win every bid.  Network latency will sometimes result in failed transactions, even if you 'out-bid' the other transaction.
* Windows Only: download and install **both** the [VC 2013 Redistributable](https://www.microsoft.com/en-ca/download/details.aspx?id=40784) and the [VC 2015 Redistributable](https://www.microsoft.com/en-ca/download/details.aspx?id=48145)
* Run `ethminer.exe --list-devices -G`.  Verify your GPU's are recognized.  Pay special attention to the PlatformID.  If it is anything other than 0, you will need to add `--opencl-platform <n>` to your command line.
* Start mining with `ethminer.exe -G -N 127.0.0.1:8545`.  This assumes the node is on  the same machine as the miner.

#### Configuration Details ####

*MVis-ethminer* is partially configured via command line parameters, and partially by settings in `ethminer.ini`.  Run `ethminer --help` to see which settings are available on the command line.  Have a look inside the .ini file to see what settings can be configured there. (It is fairly well commented).  Some settings can *only* be set on the command line (legacy ones mostly), some settings can *only* be set in the .ini file (newer ones mostly), and some can be set in both.  For the last group, command line settings take precedence over the .ini file settings.

#### Command Line Options ####

```
Node configuration:
    -N, --node <host:rpc_port>  Host address and RPC port of your node. 
        (default: 127.0.0.1:8545)
    -I, --polling-interval <n>  If using getWork (polling) to obtain work packages, check 
        for new work every <n> milliseconds (default: 200). Does not apply to stratum 
        mode (-S).

 Benchmarking mode:
    -M [<n>],--benchmark [<n>] Benchmark for mining and exit; Optionally specify block 
        number to benchmark against specific DAG.
    --benchmark-warmup <seconds>  Set the duration of warmup for the benchmark tests 
      (default: 8).
    --benchmark-trial <seconds>  Set the duration for each trial for the benchmark tests
      (default: 3).
    --benchmark-trials <n>  Set the number of benchmark tests (default: 5).

 Mining configuration:
    -C,--cpu  CPU mining
    -G,--opencl  When mining use the GPU via OpenCL.
    --cl-local-work <n> Set the OpenCL local work size. Default is 128
    --cl-work-multiplier <n> This value multiplied by the cl-local-work value equals 
       the number of hashes computed per kernel run (ie. global work size). (Default: 8192)
    --opencl-platform <n>  When mining using -G/--opencl use OpenCL platform n (default: 0).
    --opencl-device <n>  When mining using -G/--opencl use OpenCL device n (default: 0).
    --opencl-devices <0 1 ..n> Select which OpenCL devices to mine on. Default: all
    -t, --mining-threads <n> Limit number of CPU/GPU miners to n (default: all)
    --allow-opencl-cpu  Allows CPU to be considered as an OpenCL device if 
      the OpenCL platform supports it.
    --list-devices List the detected OpenCL/CUDA devices and exit. Should be 
      combined with -G or -U flag
    --cl-extragpu-mem <n> Set the memory (in MB) you believe your GPU requires 
       for stuff other than mining. default: 0

 Miscellaneous Options:
    --config <FileSpec>  - Full path to an INI file containing program options. 
       Windows default: %LocalAppData%/ethminer/ethminer.ini.  
       Linux default: $HOME/.config/ethminer/ethminer.ini.  If this option is 
       specified, it must appear before all others.
 
General Options:
    -V,--version  Show the version and exit.
    -h,--help  Show this help message and exit.
```


### Building on Windows

- download and install **Visual Studio 2015**
- download and install **CMake**
- download or clone this repository into a folder. Let's call it `<mvis_folder>`.
- run `getstuff.bat` in the extdep folder.
- open a command prompt in `<mvis_folder>` and enter the following commands.  (Note: the string "*Visual Studio 12 2013 Win64*" is **not** a typo.)

``` 
mkdir build 
cd build
cmake -G "Visual Studio 12 2013 Win64" ..
```

##### Visual Studio

- Use Visual Studio to open `mvis-ethminer.sln` located in the `build` directory.
- Visual Studio will offer to convert the project files from 2013 format to 2015 format.  **Do not accept this!!**  Click Cancel to keep the projects using the VS2013 toolset.
- Set `ethminer` as the startup project by right-clicking on it in the project pane.
- Build. Run


### Building on Ubuntu

Ubuntu, OpenCL only (**for AMD cards**)

```bash
sudo apt-get update
sudo apt-get -y install software-properties-common
sudo add-apt-repository -y ppa:ethereum/ethereum
sudo apt-get update
sudo apt-get install git cmake libcryptopp-dev libleveldb-dev libjsoncpp-dev libjsonrpccpp-dev libboost-all-dev libgmp-dev libreadline-dev libcurl4-gnutls-dev ocl-icd-libopencl1 opencl-headers mesa-common-dev libmicrohttpd-dev build-essential -y
git clone https://github.com/mining-visualizer/MVis-ethminer.git <mvis_folder>

- download the AMD ADL SDK from the AMD website, and extract it to a temporary folder
- copy all 3 .h files from the <adl_package>/include/ folder to <mvis_folder>/extdep/include/amd_adl/  
  (create subfolders as necessary)

cd <mvis_folder>
mkdir build
cd build
cmake -DBUNDLE=miner ..
make
```

You can then find the executable in the `build/ethminer` subfolder


### Donations

Please consider a donation to `mining-visualizer.eth (0xA804e933301AA2C919D3a9834082Cddda877C205)`
