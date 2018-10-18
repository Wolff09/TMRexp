#pragma once

#include <memory>
#include <mutex>
#include "prog.hpp"
#include "shape.hpp"
#include "observer.hpp"
#include "post.hpp"
#include "encoding.hpp"

namespace tmr {

	class RemainingWork {
		private:
			std::set<const Cfg*> _work;
			Encoding& _enc;
		public:
			RemainingWork(Encoding& enc);
			RemainingWork(const RemainingWork& rw) = delete;
			std::size_t size() const { return _work.size(); }
			bool done() const { return _work.empty(); }
			const Cfg& pop();
			void add(Cfg&& cfg);
			void add(std::vector<Cfg>&& cfgs) {
				for (Cfg& c : cfgs) add(std::move(c));
			}
	};


	std::unique_ptr<Encoding> fixed_point(const Program& prog, const Observer& smrobs, const Observer& threadobs);

}