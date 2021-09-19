#pragma once
#include <string>
#include <optional>
#include <vector>

namespace pbn
{
	namespace cpu
	{
		struct CPUGroup
		{
			uint32_t groupNo;
			uint32_t maximumCores;
			uint32_t activeCores;
			size_t activeCoresMask;
		};

		class CPU
		{
			// forward declarations
			class CPU_Internal;

		public:
			// getters
			static std::string Vendor(void);
			static std::string Brand(void);

			static bool SSE3(void);
			static bool PCLMULQDQ(void);
			static bool MONITOR(void);
			static bool SSSE3(void);
			static bool FMA(void);
			static bool CMPXCHG16B(void);
			static bool SSE41(void);
			static bool SSE42(void);
			static bool MOVBE(void);
			static bool POPCNT(void);
			static bool AES(void);
			static bool XSAVE(void);
			static bool OSXSAVE(void);
			static bool AVX(void);
			static bool F16C(void);
			static bool RDRAND(void);

			static bool MSR(void);
			static bool CX8(void);
			static bool SEP(void);
			static bool CMOV(void);
			static bool CLFSH(void);
			static bool MMX(void);
			static bool FXSR(void);
			static bool SSE(void);
			static bool SSE2(void);

			//Hyper threading technology
			static bool HTT(void);

			static bool FSGSBASE(void);
			static bool BMI1(void);
			static bool HLE(void);
			static bool AVX2(void);
			static bool BMI2(void);
			static bool ERMS(void);
			static bool INVPCID(void);
			static bool RTM(void);
			static bool AVX512F(void);
			static bool RDSEED(void);
			static bool ADX(void);
			static bool AVX512PF(void);
			static bool AVX512ER(void);
			static bool AVX512CD(void);
			static bool SHA(void);

			static bool PREFETCHWT1(void);

			static bool LAHF(void);
			static bool LZCNT(void);
			static bool ABM(void);
			static bool SSE4a(void);
			static bool XOP(void);
			static bool TBM(void);

			static bool SYSCALL(void);
			static bool MMXEXT(void);
			static bool RDTSCP(void);
			static bool _3DNOWEXT(void);
			static bool _3DNOW(void);


			static size_t NumaNodes();
			static size_t Packages(void);
			static size_t Cores(void);
			static size_t LogicalCores(void);


			static size_t L1CacheCount(void);
			static size_t L2CacheCount(void);
			static size_t L3CacheCount(void);

			static size_t L1CacheSize(void);
			static size_t L2CacheSize(void);
			static size_t L3CacheSize(void);


			static size_t GroupsCount(void);
			static const std::vector<CPUGroup>& Groups();


			static bool SetThreadGroupAffinity(size_t group);
			
		};


	



		inline std::string GetCPUName()
		{
			return CPU::Brand();
		}

		//Uses prdefined cpu flags from msvc
		inline bool CheckCPUOK()
		{
#if  defined (__AVX512F__)
			if (!CPU::AVX512F())
			{
				printf("Unsupported CPU platform. AVX512F Required but not supported.\n");
				return false;
			}
#elif defined (__AVX2__)
			if (!CPU::AVX2())
			{
				printf("Unsupported CPU platform. AVX2 Required but not supported.\n");
				return false;
			}
#elif defined (__AVX__)
			if (!CPU::AVX())
			{
				printf("Unsupported CPU platform. AVX Required but not supported.\n");
				return false;
			}
#elif defined (_M_IX86_FP)
#if (_M_IX86_FP==1)
			if (!CPU::SSE())
			{
				printf("Unsupported CPU platform. SSE Required but not supported.\n");
				return false;
			}
#elif(_M_IX86_FP == 2)
			if (!CPU::SSE2())
			{
				printf("Unsupported CPU platform. SSE2 Required but not supported.\n");
				return false;
			}
#endif
#endif

			return true;
		}
	}
}