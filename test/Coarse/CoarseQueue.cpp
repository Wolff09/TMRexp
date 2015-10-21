#include <stdexcept>
#include <string>
#include <iostream>
#include <chrono>
#include "prog.hpp"
#include "ObserverFactory.hpp"
#include "Coarse/Factory.hpp"
#include "conformance.hpp"
#include "counter.hpp"

using namespace tmr;


const Function& find(const Program& prog, std::string name) {
	for (std::size_t i = 0; i < prog.size(); i++)
		if (prog.at(i).name() == name)
			return prog.at(i);
	throw std::logic_error("Invalid function name lookup.");
}


int main(int argc, char *argv[]) {
	// default setup
	bool use_mega_malloc = false;
	MemorySetup msetup = PRF;

	enum Expectation { SUCCESS, FAILURE, UNKNOWN };
	Expectation expec = UNKNOWN;

	// read setup from command line
	for (int i = 1; i < argc; i++) {
		std::string cla = argv[i];
		if (cla == "--PRF") msetup = PRF;
		else if (cla == "--GC") msetup = GC;
		else if (cla == "--MM") msetup = MM;
		else if (cla == "--init") use_mega_malloc = true;
		else if (cla == "--malloc") use_mega_malloc = false;
		else if (cla == "--fail") expec = FAILURE;
		else if (cla == "--success") expec = SUCCESS;
		else {
			if (cla != "--help" && cla != "help") std::cout << "unrecognized command line argument: " << cla << std::endl;
			std::cout << std::endl << "Usage: ";
			std::cout << argv[0];
			std::cout << " [--PRF]";
			std::cout << " [--GC]";
			std::cout << " [--MM]";
			std::cout << " [--malloc/--init]";
			std::cout << " [--fail/--success]";
			std::cout << std::endl << std::endl;
			std::cout << "default: --PRF --malloc";
			std::cout << std::endl << std::endl;
			std::cout << "--PRF       => select Pointer Race Free semantics" << std::endl;
			std::cout << "--GC        => select Garbage Collection semantics" << std::endl;
			std::cout << "--MM        => select Memory Managed semantics" << std::endl;
			std::cout << "--init      => malloc (atomically) populates the allocated cells data and next fields" << std::endl;
			std::cout << "--malloc    => use regular malloc (requests memory without initialising data and next fields)" << std::endl;
			std::cout << "--fail      => emits return code 0 iff. the program is proven incorrect (for whatever reason)" << std::endl;
			std::cout << "--success   => emits return code 0 iff. the program is proven correct" << std::endl;
			std::cout << std::endl;
			return 1;
		}
	}

	// make program and observer
	std::unique_ptr<Program> program = coarse_queue(use_mega_malloc);
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());
	
	// print setup
	std::cout << std::endl << *program << std::endl;
	std::cout << "Memory Semantics: " << msetup << std::endl;
	std::cout << "Malloc vs. Init : " << (use_mega_malloc ? "atomic init" : "malloc") << std::endl;
	std::cout << std::endl;

	// execute conformance check
	auto t_start = std::chrono::high_resolution_clock::now();
	CCResult result = check_conformance(*program, *observer, msetup);
	auto t_end = std::chrono::high_resolution_clock::now();
	std::string answer = result.conformance ?  "  CORRECT" : "INCORRECT";

	// print results
	std::cout << std::endl;
	std::cout << "***********************************************************" << std::endl;
	std::cout << "**     CONFORMACE CHECK DONE, PROGRAM IS: " << answer << "!     **" << std::endl;
	std::cout << "***********************************************************" << std::endl << std::endl;

	if (!result.conformance)
		std::cout << std::endl << "Reason: " << std::endl << "    " << result.reason << std::endl << std::endl;

	
	auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
	std::cout << std::endl << "CONDENSED OUTPUT:\tCoarseQueue";
	for (int i = 1; i < argc; i++) {
		std::string cla = argv[i];
		std::cout << cla << "\t";
	}
	std::cout << answer;
	std::cout << "\t";
	if (result.conformance) std::cout << "enc.size()" << (result.conformance ? "=" : ">") << result.encoding->size();
	if (result.conformance) std::cout << "\tbuckets=" << std::distance(result.encoding->begin(), result.encoding->end());
	std::cout << "\tSC=" << SEQUENTIAL_STEPS << "\tIC=" << INTERFERENCE_STEPS << "\tIS=" << INTERFERENCE_SKIPPED;
	std::cout << "\ttime=" << time_taken/1000.0 << "s\t";
	if (!result.conformance) std::cout << "error: " << result.reason;
	std::cout << std::endl << std::endl;

	
	switch(expec) {
		case UNKNOWN: return 0;
		case SUCCESS: return result.conformance ? 0 : 1;
		case FAILURE: return result.conformance ? 1 : 0;
	}
}

