#pragma once

#include <stdexcept>
#include <string>
#include <iostream>
#include <chrono>
#include "prog.hpp"
#include "ObserverFactory.hpp"
#include "conformance.hpp"
#include "counter.hpp"
#include "config.hpp"


namespace tmr {

	static const Function& find(const Program& prog, std::string name) {
		for (std::size_t i = 0; i < prog.size(); i++)
			if (prog.at(i).name() == name)
				return prog.at(i);
		throw std::logic_error("Invalid function name lookup.");
	}


	int run(const Program& program, const Observer& linobserver, bool expect_success=true) {
		// print setup
		std::cout << "***********************************************************" << std::endl;
		std::cout << "**                         SETUP                         **" << std::endl;
		std::cout << "***********************************************************" << std::endl << std::endl;
		std::cout << "Running in ";
		std::cout << "INTERFERENCE";
		std::cout << " mode for EXPLICIT MEMORY MANAGEMENT" << std::endl << std::endl;
		std::cout << std::endl << program << std::endl;

		// execute conformance check
		auto t_start = std::chrono::high_resolution_clock::now();
		CCResult result = check_conformance(program, linobserver);
		auto t_end = std::chrono::high_resolution_clock::now();
		std::string answer = result.conformance ?  "  CORRECT" : "INCORRECT";

		// print results
		std::cout << std::endl;
		std::cout << "***********************************************************" << std::endl;
		std::cout << "**     CONFORMACE CHECK DONE, PROGRAM IS: " << answer << "!     **" << std::endl;
		std::cout << "***********************************************************" << std::endl << std::endl;
		std::cerr << "==> " << answer << std::endl;

		if (!result.conformance)
			std::cout << std::endl << "Reason: " << std::endl << "    " << result.reason << std::endl << std::endl;


		auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
		std::cout << "Time taken: " << time_taken/1000.0 << "s" << std::endl << std::endl;
		
		// std::cout << std::endl << "CONDENSED OUTPUT:\tTreibersStack";
		// std::cout << answer;
		// std::cout << "\t";
		// if (result.conformance) std::cout << "enc.size()" << (result.conformance ? "=" : ">") << result.encoding->size();
		// std::cout << "\tSC=" << SEQUENTIAL_STEPS << "\tIC=" << INTERFERENCE_STEPS << "\tIS=" << INTERFERENCE_SKIPPED;
		// if (!result.conformance) std::cout << "error: " << result.reason;
		// std::cout << std::endl << std::endl;

		return result.conformance == expect_success ? 0 : 1;
	}

}