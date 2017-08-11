/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file EthashCPUMiner.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Determines the PoW algorithm.
 */

#include "EthashCPUMiner.h"
#include <thread>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <random>
#if ETH_CPUID || !ETH_TRUE
#define HAVE_STDINT_H
#include <libcpuid/libcpuid.h>
#endif
using namespace std;
using namespace dev;
using namespace eth;

unsigned EthashCPUMiner::s_numInstances = 0;

#if ETH_CPUID || !ETH_TRUE
static string jsonEncode(map<string, string> const& _m)
{
	string ret = "{";

	for (auto const& i: _m)
	{
		string k = boost::replace_all_copy(boost::replace_all_copy(i.first, "\\", "\\\\"), "'", "\\'");
		string v = boost::replace_all_copy(boost::replace_all_copy(i.second, "\\", "\\\\"), "'", "\\'");
		if (ret.size() > 1)
			ret += ", ";
		ret += "\"" + k + "\":\"" + v + "\"";
	}

	return ret + "}";
}
#endif

EthashCPUMiner::EthashCPUMiner(Farm* _farm, unsigned _index):
	GenericMiner<EthashProofOfWork>(_farm, _index), Worker("miner" + toString(index()))
{
}

EthashCPUMiner::~EthashCPUMiner()
{
}

void EthashCPUMiner::kickOff()
{
	LogF << "Trace: EthashCPUMiner::kickOff";
	startWorking();
}

void EthashCPUMiner::pause()
{
	LogF << "Trace: EthashCPUMiner::pause";
	stopWorking();
}

void EthashCPUMiner::workLoop()
{
	LogF << "Trace: EthashCPUMiner::workLoop";
	uint64_t const c_maxHash = ~uint64_t(0);

	auto tid = std::this_thread::get_id();
	static std::mt19937_64 s_eng((time(0) + std::hash<decltype(tid)>()(tid)));
	uint64_t bestHash = c_maxHash;

	uint64_t tryNonce = s_eng();

	WorkPackage w = work();

	h256 boundary = w.boundary;
	unsigned hashCount = 1;
	uint64_t batchCount = 0;
	Timer batchTime;

	m_farm->setIsMining(true);
	
	for (; !shouldStop(); tryNonce++, hashCount++)
	{
		Nonce n = (Nonce) (u64) tryNonce;
		EthashProofOfWork::Result r = EthashAux::eval(w.seedHash, w.headerHash, n);
		if (r.value < w.boundary && submitProof(Solution{n, r.mixHash}))
			break;

		m_currentHash = upper64OfHash(r.value);

		if (m_currentHash < bestHash)
			setBestHash(m_currentHash);

		if (m_closeHit > 0 && m_currentHash < m_closeHit)
		{
			unsigned work = std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now() - m_lastCloseHit).count();
			m_farm->reportCloseHit(m_currentHash, work, m_index);
			m_lastCloseHit = SteadyClock::now();
		}

		if (batchTime.elapsedMilliseconds() > 100)
		{
			accumulateHashes(hashCount, batchCount++);
			batchTime.restart();
			hashCount = 1;
		}
	}
}

std::string EthashCPUMiner::platformInfo()
{
	string baseline = toString(std::thread::hardware_concurrency()) + "-thread CPU";
#if ETH_CPUID || !ETH_TRUE
	if (!cpuid_present())
		return baseline;
	struct cpu_raw_data_t raw;
	struct cpu_id_t data;
	if (cpuid_get_raw_data(&raw) < 0)
		return baseline;
	if (cpu_identify(&raw, &data) < 0)
		return baseline;
	map<string, string> m;
	m["vendor"] = data.vendor_str;
	m["codename"] = data.cpu_codename;
	m["brand"] = data.brand_str;
	m["L1 cache"] = toString(data.l1_data_cache);
	m["L2 cache"] = toString(data.l2_cache);
	m["L3 cache"] = toString(data.l3_cache);
	m["cores"] = toString(data.num_cores);
	m["threads"] = toString(data.num_logical_cpus);
	m["clocknominal"] = toString(cpu_clock_by_os());
	m["clocktested"] = toString(cpu_clock_measure(200, 0));
	/*
	printf("  MMX         : %s\n", data.flags[CPU_FEATURE_MMX] ? "present" : "absent");
	printf("  MMX-extended: %s\n", data.flags[CPU_FEATURE_MMXEXT] ? "present" : "absent");
	printf("  SSE         : %s\n", data.flags[CPU_FEATURE_SSE] ? "present" : "absent");
	printf("  SSE2        : %s\n", data.flags[CPU_FEATURE_SSE2] ? "present" : "absent");
	printf("  3DNow!      : %s\n", data.flags[CPU_FEATURE_3DNOW] ? "present" : "absent");
	*/
	return jsonEncode(m);
#else
	return baseline;
#endif
}
