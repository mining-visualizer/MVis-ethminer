## MVis-ethminer

This is a fork of Genoil's ethminer-0.9.41-genoil-1.x.x, which was a fork of the Ethereum Foundation ethminer.  It is part of the [Mining Visualizer](https://github.com/mining-visualizer/Mining-Visualizer) suite of programs.


### Download

Please visit the [Releases page](https://github.com/mining-visualizer/MVis-ethminer/releases) for the latest binary downloads

### Usage

Please visit the [Miner page](https://github.com/mining-visualizer/Mining-Visualizer/wiki/Miner) of the [MVis wiki](https://github.com/mining-visualizer/Mining-Visualizer/wiki) for full documentation.

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
- copy all 3 .h files from the <adl_package>/include/ folder to <mvis_folder>/extdep/include/amd_adl/  (create subfolders as necessary)

cd <mvis_folder>
mkdir build
cd build
cmake -DBUNDLE=miner ..
make
```

You can then find the executable in the `build/ethminer` subfolder

