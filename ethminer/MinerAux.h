#pragma once

/*
	This file is part of mvis-ethereum.

	mvis-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	mvis-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with mvis-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file MinerAux.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * CLI module for mining.
 */

#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <random>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/optional.hpp>

#include <libdevcore/FileSystem.h>
#include <libdevcore/StructuredLogger.h>
#include <libethcore/Exceptions.h>
#include <libdevcore/SHA3.h>
#include <libdevcore/CommonJS.h>
#include <libethcore/EthashAux.h>
#include <libethcore/EthashCUDAMiner.h>
#include <libethcore/EthashGPUMiner.h>
#include <libethcore/EthashCPUMiner.h>
#include <libethcore/Farm.h>

#include <libethash-cl/ethash_cl_miner.h>

#if ETH_ETHASHCUDA
#include <libethash-cuda/ethash_cuda_miner.h>
#endif

#include <jsonrpccpp/server/connectors/httpserver.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

#include "BuildInfo.h"

#include "PhoneHome.h"
#include "FarmClient.h"

#include <libstratum/EthStratumClient.h>

#include "ProgOpt.h"
#include "Misc.h"
#include "Common.h"
#include "MultiLog.h"
#include "MVisRPC.h"

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace boost::algorithm;

#undef RETURN


/*-----------------------------------------------------------------------------------
* class MinerCLI
*----------------------------------------------------------------------------------*/
class MinerCLI
{
public:

	enum class OperationMode
	{
		None,
		Benchmark,
		Farm,
		Stratum
	};

	typedef struct
	{
		string url;
		string rpcPort;
		string stratumPort;
		string stratumPwd;
	} node_t;


	/*-----------------------------------------------------------------------------------
	* constructor
	*----------------------------------------------------------------------------------*/
	MinerCLI()
	{}


	/*-----------------------------------------------------------------------------------
	* loadIniSettings
	*----------------------------------------------------------------------------------*/
	void loadIniSettings()
	{
		// some settings can be specified both on the CLI and in the INI file.  for those that
		// can, first load up the values from the INI file, and then any values on the CLI will
		// overwrite these.

		m_nodes.clear();

		node_t node;
		// main node
		node.url = ProgOpt::Get("Node", "Host");
		node.rpcPort = ProgOpt::Get("Node", "RPCPort");
		node.stratumPort = ProgOpt::Get("Node", "StratumPort");
		node.stratumPwd = ProgOpt::Get("Node", "StratumPwd");
		m_nodes.push_back(node);

		// failover node
		node.url = ProgOpt::Get("Node2", "Host");
		node.rpcPort = ProgOpt::Get("Node2", "RPCPort");
		node.stratumPort = ProgOpt::Get("Node2", "StratumPort");
		node.stratumPwd = ProgOpt::Get("Node2", "StratumPwd");
		m_nodes.push_back(node);
	}

	/*-----------------------------------------------------------------------------------
	* parseNode
	*----------------------------------------------------------------------------------*/
	bool parseNode(char* _arg, char* _argN, string &_url, string &_port)
	{
		string s = string(_argN);
		LowerCase(s);
		// boost asio doesn't like the http:// prefix (stratum mining)
		if (s.find("http") != string::npos)
		{
			LogS << "Invalid " << _arg << " argument. Do not specify the 'http://' prefix, just IP or Host.";
			return false;
		}

		size_t p = s.find_last_of(":");
		if (p > 0)
		{
			_url = s.substr(0, p);
			if (p + 1 <= s.length())
				_port = s.substr(p + 1);
		}
		else
			_url = s;

		return true;
	}

	/*-----------------------------------------------------------------------------------
	* failOverAvailable
	*----------------------------------------------------------------------------------*/
	bool failOverAvailable()
	{
		return m_nodes[1].url != "";
	}

	/*-----------------------------------------------------------------------------------
	* interpretOption
	*----------------------------------------------------------------------------------*/
	bool interpretOption(int& i, int argc, char** argv)
	{
		string arg = argv[i];
		if ((arg == "-N" || arg == "--node") && i + 1 < argc)
		{
			if (!parseNode(argv[i], argv[++i], m_nodes[0].url, m_nodes[0].rpcPort))
				exit(-1);
		}
		else if ((arg == "-N2" || arg == "--node2") && i + 1 < argc)
		{
			if (!parseNode(argv[i], argv[++i], m_nodes[1].url, m_nodes[1].rpcPort))
				exit(-1);
		}
		else if ((arg == "-S" || arg == "--stratum-port") && i + 1 < argc)
		{
			m_nodes[0].stratumPort = argv[++i];
			if (!isDigits(m_nodes[0].stratumPort))
			{
				LogS << "Invalid " << arg << " option: Numerical port number only!";
				exit(-1);
			}
		}
		else if ((arg == "-S2" || arg == "--stratum-port2") && i + 1 < argc)
		{
			m_nodes[1].stratumPort = argv[++i];
			if (!isDigits(m_nodes[1].stratumPort))
			{
				LogS << "Invalid " << arg << " option: Numerical port number only!";
				exit(-1);
			}
		}
		else if ((arg == "-P" || arg == "--stratum-pwd") && i + 1 < argc)
		{
			m_nodes[0].stratumPwd = argv[++i];
		}
		else if ((arg == "-P2" || arg == "--stratum-pwd2") && i + 1 < argc)
		{
			m_nodes[1].stratumPwd = argv[++i];
		}
		else if ((arg == "-I" || arg == "--polling-interval") && i + 1 < argc)
			try {
				m_pollingInterval = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if ((arg == "-R" || arg == "--farm-retries") && i + 1 < argc)
			try {
				m_maxFarmRetries = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}

		else if ((arg == "--work-timeout") && i + 1 < argc)
		{
			m_worktimeout = atoi(argv[++i]);
		}
		
		else if (arg == "--opencl-platform" && i + 1 < argc)
			try {
				m_openclPlatform = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if (arg == "--opencl-devices" || arg == "--opencl-device")
			while (m_openclDeviceCount < 16 && i + 1 < argc)
			{
				try
				{
					m_openclDevices[m_openclDeviceCount] = stol(argv[++i]);
					++m_openclDeviceCount;
                }
				catch (...)
				{
					i--;
					break;
				}
			}
		else if ((arg == "--cl-work-multiplier" || arg == "--cuda-grid-size")  && i + 1 < argc)
			try {
				m_workSizeMultiplier = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if ((arg == "--cl-local-work" || arg == "--cuda-block-size") && i + 1 < argc)
			try {
				m_localWorkSize = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if (arg == "--list-devices")
			m_shouldListDevices = true;
		else if (arg == "--export-dag" && argc > i + 1)
		{
			m_exportDAG = true;
			try
			{
				m_exportDAG_blockNum = atoi(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(2);
			}
		}
		else if ((arg == "--cl-extragpu-mem" || arg == "--cuda-extragpu-mem") && i + 1 < argc)
			m_extraGPUMemory = 1000000 * stol(argv[++i]);
		else if (arg == "--allow-opencl-cpu")
			m_clAllowCPU = true;
#if ETH_ETHASHCUDA
		else if (arg == "--cuda-devices")
		{
			while (m_cudaDeviceCount < 16 && i + 1 < argc)
			{
				try
				{
					m_cudaDevices[m_cudaDeviceCount] = stol(argv[++i]);
					++m_cudaDeviceCount;
				}
				catch (...)
				{
					i--;
					break;
				}
			}
		}
		else if (arg == "--cuda-schedule" && i + 1 < argc)
		{
			string mode = argv[++i];
			if (mode == "auto") m_cudaSchedule = 0;
			else if (mode == "spin") m_cudaSchedule = 1;
			else if (mode == "yield") m_cudaSchedule = 2;
			else if (mode == "sync") m_cudaSchedule = 4;
			else
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		}
		else if (arg == "--cuda-streams" && i + 1 < argc)
			m_numStreams = stol(argv[++i]);
#endif
		else if ((arg == "-L" || arg == "--dag-load-mode") && i + 1 < argc)
		{
			string mode = argv[++i];
			if (mode == "parallel") m_dagLoadMode = DAG_LOAD_MODE_PARALLEL;
			else if (mode == "sequential") m_dagLoadMode = DAG_LOAD_MODE_SEQUENTIAL;
			else if (mode == "single")
			{
				m_dagLoadMode = DAG_LOAD_MODE_SINGLE;
				m_dagCreateDevice = stol(argv[++i]);
			}
			else
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		}
		else if (arg == "--benchmark-warmup" && i + 1 < argc)
			try {
				m_benchmarkWarmup = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if (arg == "--benchmark-trial" && i + 1 < argc)
			try {
				m_benchmarkTrial = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if (arg == "--benchmark-trials" && i + 1 < argc)
			try
			{
				m_benchmarkTrials = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		else if (arg == "-C" || arg == "--cpu")
			m_minerType = MinerType::CPU;
		else if (arg == "-G" || arg == "--opencl")
			m_minerType = MinerType::CL;
		else if (arg == "-U" || arg == "--cuda")
		{
			m_minerType = MinerType::CUDA;
		}
		else if (arg == "-X" || arg == "--cuda-opencl")
		{
			m_minerType = MinerType::Mixed;
		}
		else if (arg == "-M" || arg == "--benchmark")
		{
			m_doBenchmark = true;
			if (i + 1 < argc)
			{
				string m = boost::to_lower_copy(string(argv[++i]));
				try
				{
					m_benchmarkBlock = stol(m);
				}
				catch (...)
				{
					if (argv[i][0] == 45) { // check next arg
						i--;
					}
					else {
						LogS << "Invalid " << arg << " option: " << argv[i];
						exit(-1);
					}
				}
			}
		}

		else if ((arg == "-t" || arg == "--mining-threads") && i + 1 < argc)
		{
			try
			{
				m_miningThreads = stol(argv[++i]);
			}
			catch (...)
			{
				LogS << "Invalid " << arg << " option: " << argv[i];
				exit(-1);
			}
		}

		else if (arg == "--config")
		{
			LogS << "Invalid --config option: This must be the first option on the command line.";
			exit(-1);
		}


		else
			return false;

		return true;
	}	// interpretOption


	/*-----------------------------------------------------------------------------------
	* execute
	*----------------------------------------------------------------------------------*/
	void execute()
	{

		if (m_minerType == MinerType::Undefined)
		{
			LogS << "No miner type specfied.  Please include either -C (CPU mining) or -G (OpenCL mining) on the command line";
			exit(-1);
		}
		LogD << " ";
		LogD << "--- Program Start ---";

		// list devices
		if (m_shouldListDevices)
		{
			if (m_minerType == MinerType::CL || m_minerType == MinerType::Mixed)
				EthashGPUMiner::listDevices();
#if ETH_ETHASHCUDA
			if (m_minerType == MinerType::CUDA || m_minerType == MinerType::Mixed)
				EthashCUDAMiner::listDevices();
#endif
			if (m_minerType == MinerType::CPU)
				LogS << "--list-devices should be combined with GPU mining flag (-G for OpenCL or -U for CUDA)";
			exit(0);
		}

		// export DAG
		if (m_exportDAG) 
		{
			EthashGPUMiner::exportDAG(m_exportDAG_blockNum);
			exit(0);
		}


		// configure GPU
		if (m_minerType == MinerType::CPU)
		{
			EthashCPUMiner::setNumInstances(m_miningThreads);
		}
		else if (m_minerType == MinerType::CL || m_minerType == MinerType::Mixed)
		{
			if (m_openclDeviceCount > 0)
			{
				EthashGPUMiner::setDevices(m_openclDevices, m_openclDeviceCount);
				m_miningThreads = m_openclDeviceCount;
			}
			
			if (!EthashGPUMiner::configureGPU(
					m_localWorkSize,
					m_workSizeMultiplier,
					m_openclPlatform,
					m_openclDevice,
					m_clAllowCPU,
					m_extraGPUMemory,
					0,
					m_dagLoadMode,
					m_dagCreateDevice
				))
				exit(1);
			EthashGPUMiner::setNumInstances(m_miningThreads);
		}
		else if (m_minerType == MinerType::CUDA || m_minerType == MinerType::Mixed)
		{
#if ETH_ETHASHCUDA
			if (m_cudaDeviceCount > 0)
			{
				EthashCUDAMiner::setDevices(m_cudaDevices, m_cudaDeviceCount);
				m_miningThreads = m_cudaDeviceCount;
			}
			
			EthashCUDAMiner::setNumInstances(m_miningThreads);
			if (!EthashCUDAMiner::configureGPU(
				m_localWorkSize,
				m_workSizeMultiplier,
				m_numStreams,
				m_extraGPUMemory,
				m_cudaSchedule,
				0,
				m_dagLoadMode,
				m_dagCreateDevice
				))
				exit(1);
#else
			cerr << "Selected CUDA mining without having compiled with -DETHASHCUDA=1 or -DBUNDLE=cudaminer" << endl;
			exit(1);
#endif
		}

		if (m_doBenchmark)
			doBenchmark(m_minerType, m_benchmarkWarmup, m_benchmarkTrial, m_benchmarkTrials);
		else
		{
			GenericFarm<EthashProofOfWork> f;
			f.start(createMiners(m_minerType, &f));
			mvisRPC = new MVisRPC(f);

			int i = 0;
			while (true)
			{
				if (m_nodes[i].url != "")
				{
					if (m_nodes[i].stratumPort != "")
						doStratum(f, m_nodes[i].url, m_nodes[i].rpcPort, m_nodes[i].stratumPort, m_nodes[i].stratumPwd);
					else
						doFarm(f, m_nodes[i].url, m_nodes[i].rpcPort);
				}
				LogB << "Switching to failover node";
				i = ++i % 2;
			}
		}
			
	}	// execute


	/*-----------------------------------------------------------------------------------
	* streamHelp
	*----------------------------------------------------------------------------------*/
	static void streamHelp(ostream& _out)
	{
		_out
			<< " Node configuration:" << endl
			<< "    -N, --node <host:rpc_port>  Host address and RPC port of your node. (default: 127.0.0.1:8545)" << endl
			<< "    -S, --stratum-port <port>  Stratum port of your node (default: disabled).  Setting this option" << endl
			<< "        implicitly enables stratum mode, otherwise polling method is used to obtain work packages (ie. getWork)" << endl
			<< "    -P, --stratum-pwd <string>  Stratum password (default: disabled)" << endl
			<< "    -N2, --node2 <host:rpc_port>  Failover node (default: disabled)" << endl
			<< "    -S2, --stratum-port2 <port>  Stratum port of failover node (default: disabled)" << endl
			<< "    -P2, --stratum-pwd2 <string>  Stratum password of failover node (default: disabled)" << endl
			<< "    -I, --polling-interval <n>  If using getWork (polling) to obtain work packages, check for new work" << endl
			<< "        every <n> milliseconds (default: 200). Does not apply to stratum mode (-S)." << endl
			<< "    --work-timeout <n> In stratum mode, if more than <n> seconds go by with no new work package, attempt" << endl
			<< "        to reconnect, or, if a failover node is available, switch to the failover.  Defaults to 180. Don't" << endl
			<< "        set lower than max. avg. block time" << endl
			<< "    -R, --farm-retries <n> Number of retries until switch to failover (default: 4)" << endl
			<< endl
			<< " Benchmarking mode:" << endl
			<< "    -M [<n>],--benchmark [<n>] Benchmark for mining and exit; Optionally specify block number to benchmark" << endl
			<< "        against specific DAG." << endl
			<< "    --benchmark-warmup <seconds>  Set the duration of warmup for the benchmark tests (default: 8)." << endl
			<< "    --benchmark-trial <seconds>  Set the duration for each trial for the benchmark tests (default: 3)." << endl
			<< "    --benchmark-trials <n>  Set the number of benchmark tests (default: 5)." << endl
			<< endl
			<< " Mining configuration:" << endl
			<< "    -C,--cpu  CPU mining" << endl
			<< "    -G,--opencl  When mining use the GPU via OpenCL." << endl
			<< "    --cl-local-work <n> Set the OpenCL local work size. Default is " << toString(ethash_cl_miner::c_defaultLocalWorkSize) << endl
			<< "    --cl-work-multiplier <n> This setting times cl-local-work equals the number of hashes computed per kernel run (ie. global " << endl
			<< "       work size). (Default: " << toString(ethash_cl_miner::c_defaultWorkSizeMultiplier) << ")" << endl
#if ETH_ETHASHCUDA
			<< "    -U,--cuda  When mining use the GPU via CUDA." << endl
			<< "    -X,--cuda-opencl Use OpenCL + CUDA in a system with mixed AMD/Nvidia cards. May require setting --opencl-platform 1" << endl
#endif
			<< "    --opencl-platform <n>  When mining using -G/--opencl use OpenCL platform n (default: 0)." << endl
			<< "    --opencl-device <n>  When mining using -G/--opencl use OpenCL device n (default: 0)." << endl
			<< "    --opencl-devices <0 1 ..n> Select which OpenCL devices to mine on. Default is to use all" << endl
			<< "    -t, --mining-threads <n> Limit number of CPU/GPU miners to n (default: use everything available on selected platform)" << endl
			<< "    --allow-opencl-cpu  Allows CPU to be considered as an OpenCL device if the OpenCL platform supports it." << endl
			<< "    --list-devices List the detected OpenCL/CUDA devices and exit. Should be combined with -G or -U flag" << endl
			<< "    -L, --dag-load-mode <mode> DAG generation mode." << endl
			<< "            parallel    - load DAG on all GPUs at the same time (default)" << endl
			<< "            sequential  - load DAG on GPUs one after another. Use this when the miner crashes during DAG generation" << endl
			<< "            single <n>  - generate DAG on device n, then copy to other devices" << endl
			<< "    --cl-extragpu-mem <n> Set the memory (in MB) you believe your GPU requires for stuff other than mining. default: 0" << endl
#if ETH_ETHASHCUDA
			<< "    --cuda-extragpu-mem Set the memory (in MB) you believe your GPU requires for stuff other than mining. Windows rendering e.t.c.." << endl
			<< "    --cuda-block-size Set the CUDA block work size. Default is " << toString(ethash_cuda_miner::c_defaultBlockSize) << endl
			<< "    --cuda-grid-size Set the CUDA grid size. Default is " << toString(ethash_cuda_miner::c_defaultGridSize) << endl
			<< "    --cuda-streams Set the number of CUDA streams. Default is " << toString(ethash_cuda_miner::c_defaultNumStreams) << endl
			<< "    --cuda-schedule <mode> Set the schedule mode for CUDA threads waiting for CUDA devices to finish work. Default is 'sync'. Possible values are:" << endl
			<< "            auto  - Uses a heuristic based on the number of active CUDA contexts in the process C and the number of logical processors in the system P. If C > P, then yield else spin." << endl
			<< "            spin  - Instruct CUDA to actively spin when waiting for results from the device." << endl
			<< "            yield - Instruct CUDA to yield its thread when waiting for results from the device." << endl
			<< "            sync  - Instruct CUDA to block the CPU thread on a synchronization primitive when waiting for the results from the device." << endl
			<< "    --cuda-devices <0 1 ..n> Select which CUDA GPUs to mine on. Default is to use all" << endl
#endif
			<< endl
			<< " Miscellaneous Options:" << endl
			<< "    --export-dag <n>  - Generate DAG for block <n> and save as file in executable folder " << endl
			<< "    --verify-dag      - Spot check DAG entries " << endl
			<< "    --config <FileSpec>  - Full path to an INI file containing program options. Windows default: %LocalAppData%/ethminer/ethminer.ini " << endl
			<< "                           Linux default: $HOME/.config/ethminer/ethminer.ini.  If this option is specified,  it must appear before " << endl
			<< "                           all others." << endl
			;
	}	// streamHelp



	/*-----------------------------------------------------------------------------------
	* createMiners
	*----------------------------------------------------------------------------------*/
	GenericFarm<EthashProofOfWork>::miners_t createMiners(MinerType _minerType, GenericFarm<EthashProofOfWork>* _f)
	{
		unsigned instances;
		GenericFarm<EthashProofOfWork>::CreateInstanceFn create;
		GenericFarm<EthashProofOfWork>::miners_t miners;

		if (_minerType == MinerType::CPU)
		{
			instances = EthashCPUMiner::instances();
			create = [] (GenericFarm<EthashProofOfWork>* _farm, unsigned _index) { return new EthashCPUMiner(_farm, _index); };
		}
		else if (_minerType == MinerType::CL)
		{
			instances = EthashGPUMiner::instances();
			create = [] (GenericFarm<EthashProofOfWork>* _farm, unsigned _index) { return new EthashGPUMiner(_farm, _index); };
		}
		else if (_minerType == MinerType::CUDA)
		{
#if ETH_ETHASHCUDA
			instances = EthashCUDAMiner::instances();
			create = [] (GenericFarm* _farm, unsigned _index) { return new EthashCUDAMiner(_farm, _index); };
#endif
		}

		for (unsigned i = 0; i < instances; ++i)
			miners.push_back(create(_f, i));

		return miners;
	}


private:

	/*-----------------------------------------------------------------------------------
	* doBenchmark
	*----------------------------------------------------------------------------------*/
	void doBenchmark(MinerType _m, unsigned _warmupDuration = 8, unsigned _trialDuration = 3, unsigned _trials = 5)
	{
		Ethash::BlockHeader genesis;
		genesis.setNumber(m_benchmarkBlock);
		genesis.setDifficulty(1 << 18);

		GenericFarm<EthashProofOfWork> f;
		f.onSolutionFound([&](EthashProofOfWork::Solution, int) { return false; });

		string platformInfo = _m == MinerType::CPU ? "CPU" : (_m == MinerType::CL ? "CL" : "CUDA");
		LogS << "Benchmarking on platform: " << platformInfo;

		LogS << "Preparing DAG for block #" << m_benchmarkBlock;

		genesis.setDifficulty(u256(1) << 63);
		f.setWork(genesis);
		f.start(createMiners(_m, &f));

		vector<double> results;
		double mean = 0;
		double innerMean = 0;

		while (!f.isMining())
			this_thread::sleep_for(chrono::milliseconds(1000));

		LogS << "Warming up...";
		for (unsigned i = 0; i <= _warmupDuration; i++)
		{
			f.hashRates().update();
			this_thread::sleep_for(chrono::seconds(1));
		}
		for (unsigned i = 0; i < _trials; ++i)
		{
			cout << "Trial " << i+1 << "... ";
			for (unsigned s = 0; s < _trialDuration; s++)
			{
				this_thread::sleep_for(chrono::milliseconds(1000));
				f.hashRates().update();
			}
			cout << f.hashRates().farmRate() << endl;
			results.push_back(f.hashRates().farmRate());
			mean += results.back();
		}
		f.stop();
		sort(results.begin(), results.end());
		int j = -1;
		for (auto const& r : results)
			if (++j > 0 && j < (int) _trials - 1)
				innerMean += r;
		innerMean /= (_trials - 2);
		cout << "min/mean/max: " << results.front() << "/" << (mean / _trials) << "/" << results.back() << " MH/s" << endl;
		cout << "inner mean: " << innerMean << " MH/s" << endl;

		exit(0);
	}	// doBenchmark


	/*-----------------------------------------------------------------------------------
	* elapsedSeconds
	*----------------------------------------------------------------------------------*/
	std::string elapsedSeconds(Timer _time)
	{
		unsigned seconds = _time.elapsedSeconds();
		std::string s = "0" + std::to_string(seconds % 60);
		return std::to_string(seconds / 60) + ":" + s.substr(s.length() - 2);
	}	// elapsedSeconds
	
	/*-----------------------------------------------------------------------------------
	* positionedOutput
	*----------------------------------------------------------------------------------*/
	void positionedOutput(GenericFarm<EthashProofOfWork> &f, Timer lastBlockTime)
	{
		f.hashRates().update();
		LogXY(1, 1) << "Rates:" << f.hashRates() << " | Temp: " << f.getMinerTemps() << " | Fan: " << f.getFanSpeeds() << "         ";
		LogXY(1, 2) << "Block #: " << f.currentBlock << " | Block time: " << elapsedSeconds(lastBlockTime)
			<< " | Target: " << upper64OfHash(f.boundary()) << "         ";
		LogXY(1, 3) << "Best hash: " << f.bestHash() << " | " << f.getCloseHits() << " | Solutions: " << f.getSolutionStats().getAccepts()
			<< " | Hash faults: " << f.getHashFaults() << "         ";
	}


	/*-----------------------------------------------------------------------------------
	* doFarm
	*----------------------------------------------------------------------------------*/
	void doFarm(GenericFarm<EthashProofOfWork>& f, string _nodeURL, string _rpcPort)
	{
		Timer lastHashRateDisplay;
		Timer lastBlockTime;

		unsigned farmRetries = 0;
		int maxRetries = failOverAvailable() ? m_maxFarmRetries : c_StopWorkAt;
		bool connectedToNode = false;

		LogS << "Connecting to node at " << _nodeURL + ":" + _rpcPort << " ...";
		jsonrpc::HttpClient client(_nodeURL + ":" + _rpcPort);
		::FarmClient rpc(client);
		mvisRPC->configNodeRPC(_nodeURL + ":" + _rpcPort);

		EthashProofOfWork::WorkPackage current, previous;
		while (true)
		{
			try
			{
				bool solutionFound = false;
				EthashProofOfWork::Solution solution;
				int solutionMiner;
				f.onSolutionFound([&] (EthashProofOfWork::Solution sol, int miner) {
					solution = sol;
					solutionMiner = miner;
					return solutionFound = true;
				});

				while (!solutionFound && !f.shutDown)
				{
					if (current)
					{
						if (lastHashRateDisplay.elapsedSeconds() >= 2.0 && f.isMining())
						{
							positionedOutput(f, lastBlockTime);
							lastHashRateDisplay.restart();
						}
					}

					Json::Value v = rpc.eth_getWork();
					if (!connectedToNode)
					{
						connectedToNode = true;
						LogS << "Connection established.";
					}
					h256 hh(v[0].asString());
					h256 newSeedHash(v[1].asString());
					farmRetries = 0;

					if (hh != current.headerHash)
					{
						try
						{
							f.currentBlock = mvisRPC->getBlockNumber() + 1;
						}
						catch (...) {}
						previous.headerHash = current.headerHash;
						previous.seedHash = current.seedHash;
						previous.boundary = current.boundary;
						current.headerHash = hh;
						current.seedHash = newSeedHash;
						current.boundary = h256(fromHex(v[2].asString()), h256::AlignRight);
						f.setWork(current);
						lastBlockTime.restart();
					}

					if (lastBlockTime.elapsedSeconds() > m_worktimeout && failOverAvailable())
					{
						LogB << "No new work received in " << m_worktimeout << " seconds.";
						// quick & dirty way to break out of 2 loops
						goto out;
					}

					this_thread::sleep_for(chrono::milliseconds(m_pollingInterval));
				}

				if (f.shutDown)
					break;

				LogB << "Solution found; Submitting to node ...";
				if (EthashAux::eval(current.seedHash, current.headerHash, solution.nonce).value < current.boundary)
				{
					bool ok = rpc.eth_submitWork("0x" + toString(solution.nonce), "0x" + toString(current.headerHash), "0x" + toString(solution.mixHash));
					f.solutionFound(ok ? SolutionState::Accepted : SolutionState::Rejected, false, solutionMiner);
				}
				else if (EthashAux::eval(previous.seedHash, previous.headerHash, solution.nonce).value < previous.boundary)
				{
					bool ok = rpc.eth_submitWork("0x" + toString(solution.nonce), "0x" + toString(previous.headerHash), "0x" + toString(solution.mixHash));
					f.solutionFound(ok ? SolutionState::Accepted : SolutionState::Rejected, true, solutionMiner);
				}
				else
				{
					f.solutionFound(SolutionState::Failed, NULL, solutionMiner);
				}
				current.reset();
			}
			catch (jsonrpc::JsonRpcException& e)
			{
				connectedToNode = false;
				LogS << "An error occurred communicating with the node : " << e.what();
				LogS << "Trying again in 5 seconds ...";
				farmRetries++;
				if (farmRetries == maxRetries)
				{
					// if there's a failover available, we'll switch to it, but worst case scenario, it could be 
					// unavailable as well, so at some point we should pause mining.  we'll do it here.
					current.reset();
					f.setWork(current);
					LogS << "Mining paused ...";
					if (failOverAvailable())
						break;
				}
				this_thread::sleep_for(chrono::seconds(5));
				LogS << "Connecting to node at " << _nodeURL + ":" + _rpcPort << " ...";
			}
			catch (const std::exception& e)
			{
				LogB << "Exception: MinerAux::doFarm - " << e.what();
			}
		}

out:
		mvisRPC->disconnect("notify");

	}	// doFarm


	/*-----------------------------------------------------------------------------------
	* doStratum
	*----------------------------------------------------------------------------------*/
	void doStratum(GenericFarm<EthashProofOfWork>& f, string _nodeURL, string _rpcPort, string _stratumPort, string _stratumPwd)
	{
		// retry of zero means retry forever, since there is no failover.
		int maxRetries = failOverAvailable() ? m_maxFarmRetries : 0;
		EthStratumClient client(&f, m_minerType, _nodeURL, _stratumPort, _stratumPwd, maxRetries, m_worktimeout);
		mvisRPC->configNodeRPC(_nodeURL + ":" + _rpcPort);

		Timer lastHashRateDisplay;
		Timer lastBlockTime;

		client.onWorkPackage([&] (unsigned int _blockNumber) {
			f.currentBlock = _blockNumber;
			lastBlockTime.restart();
		});

		while (client.isRunning())
		{
			if (lastHashRateDisplay.elapsedSeconds() >= 2.0 && client.isConnected() && f.isMining())
			{
				positionedOutput(f, lastBlockTime);
				lastHashRateDisplay.restart();
			}
			this_thread::sleep_for(chrono::milliseconds(200));
		}

		mvisRPC->disconnect("notify");

	}	// doStratum

public:

	MVisRPC* mvisRPC;

private:

	/// Mining options
	MinerType m_minerType = MinerType::Undefined;
	unsigned m_openclPlatform = 0;
	unsigned m_openclDevice = 0;
	unsigned m_miningThreads = UINT_MAX;
	bool m_shouldListDevices = false;
	bool m_exportDAG = false;
	unsigned m_exportDAG_blockNum;
	bool m_clAllowCPU = false;
#if ETH_ETHASHCL || !ETH_TRUE
	unsigned m_openclDeviceCount = 0;
	unsigned m_openclDevices[16];
#if !ETH_ETHASHCUDA || !ETH_TRUE
	unsigned m_workSizeMultiplier = ethash_cl_miner::c_defaultWorkSizeMultiplier;
	unsigned m_localWorkSize = ethash_cl_miner::c_defaultLocalWorkSize;
#endif
#endif
#if ETH_ETHASHCUDA || !ETH_TRUE
	unsigned m_workSizeMultiplier = ethash_cuda_miner::c_defaultGridSize;
	unsigned m_localWorkSize = ethash_cuda_miner::c_defaultBlockSize;
	unsigned m_cudaDeviceCount = 0;
	unsigned m_cudaDevices[16];
	unsigned m_numStreams = ethash_cuda_miner::c_defaultNumStreams;
	unsigned m_cudaSchedule = 4; // sync
#endif
	// default value was 350MB of GPU memory for other stuff (windows system rendering, e.t.c.)
	unsigned m_extraGPUMemory = 0;// 350000000; don't assume miners run desktops...
	unsigned m_dagLoadMode = 0; // parallel
	unsigned m_dagCreateDevice = 0;
	/// Benchmarking params
	bool m_doBenchmark = false;
	bool m_phoneHome = false;
	unsigned m_benchmarkWarmup = 8;
	unsigned m_benchmarkTrial = 3;
	unsigned m_benchmarkTrials = 5;
	unsigned m_benchmarkBlock = 0;
	
	std::vector<node_t> m_nodes;

	unsigned m_maxFarmRetries = 4;
	unsigned m_pollingInterval = 200;
	unsigned m_worktimeout = 180;
};
