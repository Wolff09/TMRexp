#pragma once

#include <string>
#include "prog.hpp"
#include "observer.hpp"
#include "post.hpp"
#include "encoding.hpp"
#include "fixpoint.hpp"


namespace tmr {

	struct CCResult {
		const bool conformance;
		const std::string reason;
		std::unique_ptr<Encoding> encoding;
		CCResult(std::unique_ptr<Encoding> enc) : conformance(true), reason(""), encoding(std::move(enc)) {}
		CCResult(std::string reason) : conformance(false), reason(reason) {}
	};

	CCResult check_conformance(const Program& program, const Observer& smrobs, const Observer& threadobs) {
		try {
			auto fp = fixed_point(program, smrobs, threadobs);
			return CCResult(std::move(fp));
		} catch (std::runtime_error& e) {
			return CCResult(e.what());
		}
	}

}