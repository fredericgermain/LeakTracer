////////////////////////////////////////////////////////
//
// LeakTracer
// Contribution to original project by Erwin S. Andreasen
// site: http://www.andreasen.org/LeakTracer/
//
// Added by Michael Gopshtein, 2006
// mgopshtein@gmail.com
//
// Any comments/suggestions are welcome
//
////////////////////////////////////////////////////////


#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "MemoryTrace.hpp"


char *doAlloc(unsigned int size);


int main()
{
	// startup part of the program
	// following allocation is not registered
	char *lostAtStartup = doAlloc(128); strcpy(lostAtStartup, "Lost at startup");

	// starting monitoring allocations
	leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
	char *memLeak = doAlloc(256); strcpy(memLeak, "This is a real memory leak");
	char *notLeak = doAlloc(64); strcpy(notLeak, "This is NOT a memory leak");

	char *memLeak2 = (char*)malloc(256); strcpy(memLeak2, "This is a malloc memory leak");
	free(lostAtStartup);

//	while (1)
//		wait();

	// stop monitoring allocations, but do still
	// monitor releases of the memory
	leaktracer::MemoryTrace::GetInstance().stopMonitoringAllocations();
	delete[] notLeak;
	notLeak = doAlloc(32);

	// stop all monitoring, print report
	leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();

	std::ofstream oleaks;
	oleaks.open("leaks.out", std::ios_base::out);
	if (oleaks.is_open())
		leaktracer::MemoryTrace::GetInstance().writeLeaks(oleaks);
	else
		std::cerr << "Failed to write to \"leaks.out\"\n";

	return 0;
}


char *doAlloc(unsigned int size) {
	return new char[size];
}


