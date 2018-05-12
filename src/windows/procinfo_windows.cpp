#include <windows.h>
#include <malloc.h>
#include <stdio.h>
#include <tchar.h>
#include <thread>
#include <stdint.h>

// from: https://msdn.microsoft.com/en-us/library/ms683194

typedef BOOL(WINAPI *LPFN_GLPI)(
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION,
	PDWORD);

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask)
{
	DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
	DWORD i;

	for (i = 0; i <= LSHIFT; ++i)
	{
		bitSetCount += ((bitMask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}

	return bitSetCount;
}

static uint32_t s_nPhysicalCores = 0;
static uint32_t s_nLogicalCores = 0;

uint32_t nPhysicalCores() {
	return s_nPhysicalCores;
}

uint32_t nLogicalCores() {
	return s_nLogicalCores;
}

bool initProcInfo(std::string& outLog)
{
	// set default values using std features
	s_nPhysicalCores = s_nLogicalCores = std::thread::hardware_concurrency();

	// try to get more info about cpu(s)
	char log[2048];
	char* pLog = log;

	LPFN_GLPI glpi;
	BOOL done = FALSE;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
	DWORD returnLength = 0;
	DWORD logicalProcessorCount = 0;
	DWORD numaNodeCount = 0;
	DWORD processorCoreCount = 0;
	DWORD processorL1CacheCount = 0;
	DWORD processorL2CacheCount = 0;
	DWORD processorL3CacheCount = 0;
	DWORD processorPackageCount = 0;
	DWORD byteOffset = 0;
	PCACHE_DESCRIPTOR Cache;

	glpi = (LPFN_GLPI)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")),
		"GetLogicalProcessorInformation");
	if (NULL == glpi)
	{
		pLog += snprintf(pLog, sizeof(log) - (pLog - log), "\nProcInfo: GetLogicalProcessorInformation is not supported.\n");
		return false;
	}

	while (!done)
	{
		DWORD rc = glpi(buffer, &returnLength);

		if (FALSE == rc)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer)
					free(buffer);

				buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
					returnLength);

				if (NULL == buffer)
				{
					pLog += snprintf(pLog, sizeof(log) - (pLog - log), "\nProcInfo Error: Allocation failure\n");
					return false;
				}
			}
			else
			{
				pLog += snprintf(pLog, sizeof(log) - (pLog - log), "\nProcInfo Error %d\n", GetLastError());
				return false;
			}
		}
		else
		{
			done = TRUE;
		}
	}

	ptr = buffer;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		switch (ptr->Relationship)
		{
		case RelationNumaNode:
			// Non-NUMA systems report a single record of this type.
			numaNodeCount++;
			break;

		case RelationProcessorCore:
			processorCoreCount++;

			// A hyperthreaded core supplies more than one logical processor.
			logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
			break;

		case RelationCache:
			// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
			Cache = &ptr->Cache;
			if (Cache->Level == 1)
			{
				processorL1CacheCount++;
			}
			else if (Cache->Level == 2)
			{
				processorL2CacheCount++;
			}
			else if (Cache->Level == 3)
			{
				processorL3CacheCount++;
			}
			break;

		case RelationProcessorPackage:
			// Logical processors share a physical package.
			processorPackageCount++;
			break;

		default:
			pLog += snprintf(pLog, sizeof(log) - (pLog - log), "\nProcInfo Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
			break;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}

	pLog += snprintf(pLog, sizeof(log) - (pLog - log), "Detected %d logical processors (%d physical processors)\n", logicalProcessorCount, processorCoreCount);
	free(buffer);

	// handle unexpected results
	if (logicalProcessorCount != std::thread::hardware_concurrency()) {
		pLog += snprintf(pLog, sizeof(log) - (pLog - log), 
			"WARNING: problem while detecting number of physical & logical cores, defaulting to %d physical/logical cores\n", s_nPhysicalCores);
	}
	// record detected number of physical / logical cores
	else {
		s_nPhysicalCores = processorCoreCount;
		s_nLogicalCores = logicalProcessorCount;
	}

	// remove last \n (logLine adds it)
	size_t l = strlen(log);
	if (l>0 && log[l-1] == '\n')
		log[l-1] = 0;

	// send log
	outLog = log;
	return true;
}