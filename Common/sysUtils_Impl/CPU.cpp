#include "CPU.h"
#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <intrin.h>
#include <Windows.h>
#include <processthreadsapi.h>





namespace pbn::cpu
{

	constexpr uint32_t CountSetBits(uint32_t n)
	{
		n = n - ((n >> 1) & 0x55555555);
		n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
		n = (n + (n >> 4)) & 0x0F0F0F0F;
		n = n + (n >> 8);
		n = n + (n >> 16);
		return n & 0x0000003F;
	}


	constexpr uint32_t CountSetBits(ULONG_PTR v)
	{
#ifdef _WIN64
		
			return CountSetBits(static_cast<uint32_t>(v & 0xFFFFFFFF)) + CountSetBits(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
#else
			return CountSetBits(static_cast<uint32_t>(v));
#endif
	}


	class CPU_Internal
	{
	private:
		void ProcessorsExtendedInfo();
	public:
		CPU_Internal();


		int nIds_;
		int nExIds_;
		std::string vendor_;
		std::string brand_;
		bool isIntel_;
		bool isAMD_;
		std::bitset<32> f_1_ECX_;
		std::bitset<32> f_1_EDX_;
		std::bitset<32> f_7_EBX_;
		std::bitset<32> f_7_ECX_;
		std::bitset<32> f_81_ECX_;
		std::bitset<32> f_81_EDX_;
		std::vector<std::array<int, 4>> data_;
		std::vector<std::array<int, 4>> extdata_;





		uint32_t numaNodesCount = 0;
		
		uint32_t processorPackageCount = 0;
		uint32_t processorCoreCount = 0;
		uint32_t logicalProcessorCount = 0;
		size_t processorL1CacheCount = 0;
		size_t processorL1CacheSize = 0;
		size_t processorL2CacheCount = 0;
		size_t processorL2CacheSize = 0;
		size_t processorL3CacheCount = 0;
		size_t processorL3CacheSize = 0;

		std::vector<CPUGroup> groups;
	};


	void CPU_Internal::ProcessorsExtendedInfo()
	{


		BOOL done = FALSE;
		//PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;

		std::vector<uint8_t> buffer;

		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ptr = NULL;
		DWORD returnLength = 0;

		DWORD byteOffset = 0;


		while (!done)
		{
			DWORD rc = GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &returnLength);
			//DWORD rc = GetLogicalProcessorInformation(reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &returnLength);

			if (FALSE == rc)
			{
				if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
				{
					buffer.resize(returnLength);
				}
				else
				{
					//Log::error(__func__, "Error: {}", GetLastError());
					return;
				}
			}
			else
			{
				done = TRUE;
			}
		}


		int64_t cbRemaining = returnLength;
		

		while (true)
		{
			ptr = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + byteOffset);
			switch (ptr->Relationship)
			{
			case RelationNumaNode:
				// Non-NUMA systems report a single record of this type.
				numaNodesCount++;
				break;

			case RelationProcessorCore:
				processorCoreCount++;
				
				// A hyperthreaded core supplies more than one logical processor.
				for (size_t i = 0; i < ptr->Processor.GroupCount; i++)
				{
					logicalProcessorCount += CountSetBits(ptr->Processor.GroupMask[i].Mask);
				}
				//
				break;

			case RelationCache:
			{
				// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache.
				auto& Cache = ptr->Cache;


				if (Cache.Level == 1)
				{
					processorL1CacheCount++;
					processorL1CacheSize += Cache.CacheSize;
				}
				else if (Cache.Level == 2)
				{
					processorL2CacheCount++;
					processorL2CacheSize += Cache.CacheSize;
				}
				else if (Cache.Level == 3)
				{
					processorL3CacheCount++;
					processorL3CacheSize += Cache.CacheSize;
				}
			}
			break;

			case RelationProcessorPackage:
				// Logical processors share a physical package.
				processorPackageCount++;
				break;

			case RelationGroup:
			{
				auto& grps = ptr->Group;				
				for (uint32_t i = 0; i < grps.MaximumGroupCount; i++)
				{
					CPUGroup& grp = groups.emplace_back();
					grp.groupNo = i;
					grp.activeCores = grps.GroupInfo[i].ActiveProcessorCount;
					grp.activeCoresMask = grps.GroupInfo[i].ActiveProcessorMask;
					grp.maximumCores = grps.GroupInfo[i].MaximumProcessorCount;				
				}
				
			}
			break;
			default:
				//Log::error(__func__,"Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.");
				//printf("TEST\n");
				break;
			}


			
			cbRemaining -= ptr->Size;
			if (cbRemaining < 1) break;
			byteOffset += ptr->Size;			
		}

	}

	CPU_Internal::CPU_Internal()
		: nIds_{ 0 },
		nExIds_{ 0 },
		isIntel_{ false },
		isAMD_{ false },
		f_1_ECX_{ 0 },
		f_1_EDX_{ 0 },
		f_7_EBX_{ 0 },
		f_7_ECX_{ 0 },
		f_81_ECX_{ 0 },
		f_81_EDX_{ 0 },
		data_{},
		extdata_{}
	{
		//int cpuInfo[4] = {-1};
		std::array<int, 4> cpui;

		// Calling __cpuid with 0x0 as the function_id argument
		// gets the number of the highest valid function ID.
		__cpuid(cpui.data(), 0);
		nIds_ = cpui[0];

		for (int i = 0; i <= nIds_; ++i)
		{
			__cpuidex(cpui.data(), i, 0);
			data_.push_back(cpui);
		}

		// Capture vendor string
		char vendor[0x20];
		memset(vendor, 0, sizeof(vendor));
		*reinterpret_cast<int*>(vendor) = data_[0][1];
		*reinterpret_cast<int*>(vendor + 4) = data_[0][3];
		*reinterpret_cast<int*>(vendor + 8) = data_[0][2];
		vendor_ = vendor;
		if (vendor_ == "GenuineIntel")
		{
			isIntel_ = true;
		}
		else if (vendor_ == "AuthenticAMD")
		{
			isAMD_ = true;
		}

		// load bitset with flags for function 0x00000001
		if (nIds_ >= 1)
		{
			f_1_ECX_ = data_[1][2];
			f_1_EDX_ = data_[1][3];
		}

		// load bitset with flags for function 0x00000007
		if (nIds_ >= 7)
		{
			f_7_EBX_ = data_[7][1];
			f_7_ECX_ = data_[7][2];
		}

		// Calling __cpuid with 0x80000000 as the function_id argument
		// gets the number of the highest valid extended ID.
		__cpuid(cpui.data(), 0x80000000);
		nExIds_ = cpui[0];

		char brand[0x40];
		memset(brand, 0, sizeof(brand));

		for (int i = 0x80000000; i <= nExIds_; ++i)
		{
			__cpuidex(cpui.data(), i, 0);
			extdata_.push_back(cpui);
		}

		// load bitset with flags for function 0x80000001
		if (nExIds_ >= 0x80000001)
		{
			f_81_ECX_ = extdata_[1][2];
			f_81_EDX_ = extdata_[1][3];
		}

		// Interpret CPU brand string if reported
		if (nExIds_ >= 0x80000004)
		{
			memcpy(brand, extdata_[2].data(), sizeof(cpui));
			memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
			memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
			brand_ = brand;
		}
		ProcessorsExtendedInfo();
	};

	static const CPU_Internal CPU_Rep;

	size_t CPU::NumaNodes(void) { return CPU_Rep.numaNodesCount; }
	size_t CPU::Packages(void) { return CPU_Rep.processorPackageCount; }
	size_t CPU::Cores(void) { return CPU_Rep.processorCoreCount; }
	size_t CPU::LogicalCores(void) { return CPU_Rep.logicalProcessorCount; }


	size_t CPU::L1CacheCount(void) { return CPU_Rep.processorL1CacheCount; }
	size_t CPU::L2CacheCount(void) { return CPU_Rep.processorL2CacheCount; }
	size_t CPU::L3CacheCount(void) { return CPU_Rep.processorL3CacheCount; }

	size_t CPU::L1CacheSize(void) { return CPU_Rep.processorL1CacheSize; }
	size_t CPU::L2CacheSize(void) { return CPU_Rep.processorL2CacheSize; }
	size_t CPU::L3CacheSize(void) { return CPU_Rep.processorL3CacheSize; }

	size_t CPU::GroupsCount(void)
	{
		return CPU_Rep.groups.size();
	}

	const std::vector<CPUGroup>& CPU::Groups()
	{
		return CPU_Rep.groups;
	}


	

	bool CPU::SetThreadGroupAffinity(size_t group)
	{
		if (group >= CPU_Rep.groups.size()) return false;

		const auto& g = CPU_Rep.groups[group];
		GROUP_AFFINITY ga{};
		ga.Group = g.groupNo;
		ga.Mask = g.activeCoresMask;
		GROUP_AFFINITY gaPrev{};


		return TRUE == ::SetThreadGroupAffinity(GetCurrentThread(), &ga, &gaPrev);
	}




	std::string CPU::Vendor(void) { return CPU_Rep.vendor_; }
	std::string CPU::Brand(void) { return CPU_Rep.brand_; }

	bool CPU::SSE3(void) { return CPU_Rep.f_1_ECX_[0]; }
	bool CPU::PCLMULQDQ(void) { return CPU_Rep.f_1_ECX_[1]; }
	bool CPU::MONITOR(void) { return CPU_Rep.f_1_ECX_[3]; }
	bool CPU::SSSE3(void) { return CPU_Rep.f_1_ECX_[9]; }
	bool CPU::FMA(void) { return CPU_Rep.f_1_ECX_[12]; }
	bool CPU::CMPXCHG16B(void) { return CPU_Rep.f_1_ECX_[13]; }
	bool CPU::SSE41(void) { return CPU_Rep.f_1_ECX_[19]; }
	bool CPU::SSE42(void) { return CPU_Rep.f_1_ECX_[20]; }
	bool CPU::MOVBE(void) { return CPU_Rep.f_1_ECX_[22]; }
	bool CPU::POPCNT(void) { return CPU_Rep.f_1_ECX_[23]; }
	bool CPU::AES(void) { return CPU_Rep.f_1_ECX_[25]; }
	bool CPU::XSAVE(void) { return CPU_Rep.f_1_ECX_[26]; }
	bool CPU::OSXSAVE(void) { return CPU_Rep.f_1_ECX_[27]; }
	bool CPU::AVX(void) { return CPU_Rep.f_1_ECX_[28]; }
	bool CPU::F16C(void) { return CPU_Rep.f_1_ECX_[29]; }
	bool CPU::RDRAND(void) { return CPU_Rep.f_1_ECX_[30]; }

	bool CPU::MSR(void) { return CPU_Rep.f_1_EDX_[5]; }
	bool CPU::CX8(void) { return CPU_Rep.f_1_EDX_[8]; }
	bool CPU::SEP(void) { return CPU_Rep.f_1_EDX_[11]; }
	bool CPU::CMOV(void) { return CPU_Rep.f_1_EDX_[15]; }
	bool CPU::CLFSH(void) { return CPU_Rep.f_1_EDX_[19]; }
	bool CPU::MMX(void) { return CPU_Rep.f_1_EDX_[23]; }
	bool CPU::FXSR(void) { return CPU_Rep.f_1_EDX_[24]; }
	bool CPU::SSE(void) { return CPU_Rep.f_1_EDX_[25]; }
	bool CPU::SSE2(void) { return CPU_Rep.f_1_EDX_[26]; }
	bool CPU::HTT(void) { return CPU_Rep.f_1_EDX_[28]; }

	bool CPU::FSGSBASE(void) { return CPU_Rep.f_7_EBX_[0]; }
	bool CPU::BMI1(void) { return CPU_Rep.f_7_EBX_[3]; }
	bool CPU::HLE(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[4]; }
	bool CPU::AVX2(void) { return CPU_Rep.f_7_EBX_[5]; }
	bool CPU::BMI2(void) { return CPU_Rep.f_7_EBX_[8]; }
	bool CPU::ERMS(void) { return CPU_Rep.f_7_EBX_[9]; }
	bool CPU::INVPCID(void) { return CPU_Rep.f_7_EBX_[10]; }
	bool CPU::RTM(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[11]; }
	bool CPU::AVX512F(void) { return CPU_Rep.f_7_EBX_[16]; }
	bool CPU::RDSEED(void) { return CPU_Rep.f_7_EBX_[18]; }
	bool CPU::ADX(void) { return CPU_Rep.f_7_EBX_[19]; }
	bool CPU::AVX512PF(void) { return CPU_Rep.f_7_EBX_[26]; }
	bool CPU::AVX512ER(void) { return CPU_Rep.f_7_EBX_[27]; }
	bool CPU::AVX512CD(void) { return CPU_Rep.f_7_EBX_[28]; }
	bool CPU::SHA(void) { return CPU_Rep.f_7_EBX_[29]; }

	bool CPU::PREFETCHWT1(void) { return CPU_Rep.f_7_ECX_[0]; }

	bool CPU::LAHF(void) { return CPU_Rep.f_81_ECX_[0]; }
	bool CPU::LZCNT(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_ECX_[5]; }
	bool CPU::ABM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[5]; }
	bool CPU::SSE4a(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[6]; }
	bool CPU::XOP(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[11]; }
	bool CPU::TBM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[21]; }

	bool CPU::SYSCALL(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[11]; }
	bool CPU::MMXEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[22]; }
	bool CPU::RDTSCP(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[27]; }
	bool CPU::_3DNOWEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[30]; }
	bool CPU::_3DNOW(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[31]; }








}