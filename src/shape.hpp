#pragma once

#include <vector>
#include "relset.hpp"
#include "prog.hpp"

#include "boost/multi_array.hpp"

namespace tmr {

	class Shape {
		private:
			typedef boost::multi_array<RelSet, 2> shape_t;
			std::size_t _numGlobVars;
			std::size_t _numLocVars;
			std::size_t _bounds;
			std::size_t _size;
			shape_t _cells;
			constexpr const static std::size_t _numObsVars=2;

		public:
			Shape(std::size_t numObsVars, std::size_t numGlobVars, std::size_t numLocVars, unsigned short numThreads);
			std::size_t size() const { return _bounds; }
			std::size_t sizeLocals() const { return _numLocVars; }
			std::size_t sizeObservers() const { return 2; }
			void print(std::ostream& os) const;
			
			// shape access
			RelSet at(std::size_t i, std::size_t j) const;
			bool test(std::size_t i, std::size_t j, Rel r) const;

			void set(std::size_t i, std::size_t j, RelSet rs);
			void set(std::size_t i, std::size_t j, Rel r) { set(i, j, singleton(r)); }

			void remove_relation(std::size_t i, std::size_t j, Rel r);
			void add_relation(std::size_t i, std::size_t j, Rel r);

			// comparisons
			bool operator<(const Shape& other) const;
			bool operator==(const Shape& other) const;

			// convert cell term to index
			/* Layout:
			 *   0:           NULL
			 *   1:           REUSE
			 *   2:           UNDEF
			 *   3 - k:       observer variables
			 *   k+1 - n:     global variables
			 *   n+1 - ...:   local variables (grouped by thread)
			 */
			inline std::size_t index_NULL() const { return 0; }
			// inline std::size_t index_FREE() const { return 1; }
			inline std::size_t index_REUSE() const { return 1; }
			inline std::size_t index_UNDEF() const { return 2; }
			std::size_t index_ObserverVar(const unsigned short id) const {
				assert(id < _numObsVars);
				return 3+id;
			}
			std::size_t index_GlobalVar(const Variable& var) const {
				assert(var.global());
				assert(var.id() < _numGlobVars);
				return 3 + _numObsVars + var.id();
			}
			std::size_t index_LocalVar(const Variable& var, unsigned short tid) const {
				assert(var.local());
				assert(var.id() < _numLocVars);
				// assert(tid < _numThreads);
				return 3 + _numObsVars + _numGlobVars + tid*_numLocVars + var.id();
			}
			inline std::size_t offset_vars() const { return 3; }
			std::size_t offset_program_vars() const { return 3+_numObsVars; }
			std::size_t offset_locals(unsigned short tid) const {
				// assert(tid < _numThreads);
				return 3 + _numObsVars + _numGlobVars + tid*_numLocVars;
			}

			void extend();
			void shrink();
	};

	std::ostream& operator<<(std::ostream& os, const Shape& shape);
}
