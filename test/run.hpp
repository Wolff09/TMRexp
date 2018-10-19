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

	int run(const Program& program, const Observer& smrobs, const Observer& threadobs, bool expect_success=true) {
		// print setup
		std::cout << "***********************************************************" << std::endl;
		std::cout << "**                         SETUP                         **" << std::endl;
		std::cout << "***********************************************************" << std::endl << std::endl;
		// TODO: name observer
		std::cout << std::endl << program << std::endl;

		// execute conformance check
		auto t_start = std::chrono::high_resolution_clock::now();
		CCResult result = check_conformance(program, smrobs, threadobs);
		auto t_end = std::chrono::high_resolution_clock::now();
		std::string answer = result.conformance ?  "CORRECT" : "INCORRECT";

		// print results
		std::cout << std::endl;
		std::cout << "***********************************************************" << std::endl;
		std::cout << "**     CONFORMACE CHECK DONE, PROGRAM IS: " << answer << "!     **" << std::endl;
		std::cout << "***********************************************************" << std::endl << std::endl;
		std::cerr << std::endl << "==> " << answer << std::endl;

		if (!result.conformance)
			std::cout << std::endl << "Reason: " << std::endl << "    " << result.reason << std::endl << std::endl;


		auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
		std::cout << "Time taken: " << time_taken/1000.0 << "s" << std::endl << std::endl;

		// gist
		std::cout << std::endl << std:: endl << std::endl << std::endl;
		std::cout << "====================== GIST ======================" << std::endl;
		std::cout << "Program:  " << program.name() << std::endl;
		std::cout << "Verdict:  " << answer << std::endl;
		std::cout << "Time:     " << time_taken/1000.0 << "s" << std::endl;
		if (result.encoding) {
			std::cout << "Encoding.size():    " << result.encoding->size() << " (" << (result.encoding->size()/1000) << "k)" << std::endl;
			std::cout << "Encoding.buckets(): " << result.encoding->bucket_count() << std::endl;
		} else {
			std::cout << "Encoding.size():    ?" << std::endl;
			std::cout << "Encoding.buckets(): ?" << std::endl;
		}
		std::cout << "Sequential Steps:   " << SEQUENTIAL_STEPS << " (" << (SEQUENTIAL_STEPS/1000) << "k)" << std::endl;
		std::cout << "Interfernece Steps: " << INTERFERENCE_STEPS << " (" << (INTERFERENCE_STEPS/1000) << "k)" << std::endl;
		std::cout << "==================================================" << std::endl;
		std::cout << std::endl << std:: endl << std::endl << std::endl;

		return result.conformance == expect_success ? 0 : 1;
	}

	int run_ebr(const Program& program, bool expect_success=true) {
		auto smrobs = ebr_observer(program);
		auto threadobs = base_observer(program);
		return run(program, *smrobs, *threadobs, expect_success);
	}

	int run_ebr_with_inv(const Program& program, bool expect_success=true) {
		auto smrobs = ebr_observer(program);
		auto threadobs = base_observer_with_EBR_assumption(program);
		return run(program, *smrobs, *threadobs, expect_success);
	}

	int run_hp(const Program& program, bool expect_success=true) {
		auto smrobs = hp_observer(program);
		auto threadobs = base_observer(program);
		return run(program, *smrobs, *threadobs, expect_success);
	}

}