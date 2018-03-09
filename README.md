## MVis-ethminer

This is a fork of Genoil's ethminer-0.9.41-genoil-1.x.x, which was a fork of the Ethereum Foundation ethminer.  It is part of the [Mining Visualizer](https://github.com/mining-visualizer/Mining-Visualizer) suite of programs, and as such the main emphasis is on solo mining.  Pool mining is currently not supported.

**0xBitcoin!!** : If you are looking for the 0xBitcoin miner, it's on the [0xBitcoin branch](https://github.com/mining-visualizer/MVis-ethminer/tree/0xbitcoin)


**Platform support** : Windows, Linux

### Distinguishing Features

* Correct hash rate reporting. The original ethminer, and Genoil's as well (I don't think he made any significant changes to this part of the code), was really bad at calculating and displaying hash rates.  The numbers would jump all over the place when in fact the actual underlying hash rate was much more constant.   
* Best hash: the miner keeps track of the best hash found since mining the last block.  In other  words, it shows the closest you have come to mining a new block.
* Close hits: in a similar vein, the miner tracks hashes found that were within a certain range of the target.  These are not actually displayed by the miner, but passed on to Mining Visualizer to be displayed in the desktop widgets and the web app.
* Work Units: identical to Close Hits, but set at a much lower difficulty level (on average, about 1 every 10 minutes).  You could compare these to the *shares* you get with pool mining, except there is no reward.  The purpose of this is simply to show that the miner is actually working.
* GPU throttling: if any of the GPUs get too hot, the miner will start inserting pauses in the hashing algorithm to keep the temperature down.  If that proves ineffective, the whole mining rig shuts down after a specified period of time.
* Positioned screen output: as opposed to continuously scrolling screen output. ([Screenshot](https://github.com/mining-visualizer/Mining-Visualizer/wiki/Miner#screen-output))
* Temperature & fan speed reporting.  Currently this is available only on Windows, but support for Linux should be coming soon.
* Hash faults: every kernel run, the CPU verifies one of the hashes computed by the GPU for correctness.  Note that this is a **much higher frequency** of checking than the original ethminer, so don't be surprised if you see higher numbers.
* MinerRPC: a RPC-UDP protocol was implemented to allow the miner to talk to Mining Visualizer.
* A few bug fixes and optimizations which might result in slightly higher hash rate.


### Download

Binaries are are available for Windows and Linux.  Please visit the [Release page](https://github.com/mining-visualizer/MVis-ethminer/releases) for the latest binary downloads

### Documentation

Please visit the [Miner page](https://github.com/mining-visualizer/Mining-Visualizer/wiki/Miner) of the [MVis wiki](https://github.com/mining-visualizer/Mining-Visualizer/wiki) for full documentation.

#### Limitations

* CUDA mining is broken.  The old code from Genoil is still there, but I don't have an NVidia device so I have not been able to modify the CUDA code to support some of the new features I have added to the miner.  Hopefully I will be able to address this in the near future, or better yet, maybe an experienced CUDA dev will step up and volunteer for this effort. :smiley:
* Pool mining is broken.  Many of the features I have added don't really make any sense with pool mining, so it is unlikely I will expend any effort on this issue.  If someone wants to submit a PR though, I would be happy to look at it.
* GPU temperatures and fan speeds are unavailable when running under Linux.  Hopefully support for this can be added soon.


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

Note: if any devs want to help update this project and build process to something more modern (that uses the VS2015 toolset) I would gladly accept submissions.


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

Donations will be gratefully accepted at the following addresses:
```
- mining-visualizer.eth
- 0xA804e933301AA2C919D3a9834082Cddda877C205 (ETH)
- 0x29224Be72851D7Bad619f64c2E51E8Ca5Ba1094b (ETC)
```