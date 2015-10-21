#pragma once

#include <memory>
#include "prog.hpp"
#include "shape.hpp"
#include "observer.hpp"
#include "post.hpp"
#include "encoding.hpp"

namespace tmr {

	class RemainingWork {
		private:
			struct debug_sorter {
				bool operator()(const Cfg* lhs, const Cfg* rhs) const;
			};
			std::set<const Cfg*, debug_sorter> _work;
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


	std::unique_ptr<Encoding> fixed_point(const Program& prog, const Observer& obs, MemorySetup msetup);

}