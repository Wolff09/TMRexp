#pragma once

#include <array>
#include <bitset>
#include <iostream>
#include <assert.h>


namespace tmr {

	enum Rel { EQ=0, MT=1, MF=2, GT=3, GF=4, BT=5 };
	static std::array<Rel, 6> RelValues{{ EQ, MT, MF, GT, GF, BT }};

	typedef std::bitset<6> RelSet;

	class RelSetIterator {
		private:
			const RelSet& _rs;
			std::size_t _index;
			RelSetIterator(const RelSet& rs, std::size_t index) : _rs(rs), _index(index) {}
		public:
			bool operator !=(const RelSetIterator& other) const {
				assert(_rs == other._rs);
				return _index != other._index;
			}
			void operator++() {
				do _index++;
				while (_index < RelValues.size() && !_rs.test(RelValues[_index]));
			}
			Rel operator*() const {
				assert(_index < RelValues.size());
				return RelValues[_index];
			}
			static RelSetIterator begin(const RelSet& rs) {
				RelSetIterator res(rs, 0);
				if (!rs.test(RelValues[0])) res.operator++();
				return res;
			}
			static RelSetIterator end(const RelSet& rs) {
				return RelSetIterator(rs, RelValues.size());
			}
	};

}

namespace std {
	static tmr::RelSetIterator begin(const tmr::RelSet& relset) { return tmr::RelSetIterator::begin(relset); }
	static tmr::RelSetIterator end(const tmr::RelSet& relset) { return tmr::RelSetIterator::end(relset); }
}

namespace tmr {

	/**
	 * @brief Gives a set containing only one element, namely the argument.
	 */
	static RelSet singleton(Rel val) {
		return RelSet(1 << val);
	}
	
	/**
	 * @brief Checks whether the given set contains the given relation.
	 */
	static bool contains(const RelSet& set, const Rel& val) {
		return set.test(val);
	}

	/**
	 * @brief Checks whether the first argument is a subset of the second argument
	 */
	static bool subset(const RelSet& set1, const RelSet set2) {
		return (set1 | set2) == set2;
	}

	/**
	 * @brief Checks whether there are elements common to both passed sets.
	 */
	static bool haveCommon(const RelSet& set1, const RelSet& set2) {
		// check whether the intersection of both sets contains any element
		return (set1 & set2).any();
	}
	
	/**
	 * @brief Gives a new set containing the elements from both passed sets.
	 */
	static RelSet setunion(const RelSet& set1, const RelSet& set2) {
		return set1 | set2;
	}

	/**
	 * @brief Gives the intersection of both passed sets.
	 */
	static RelSet intersection(const RelSet& set1, const RelSet& set2) {
		return set1 & set2;
	}

	/**
	 * @brief Computes the symmetric relation.
	 */
	static Rel symmetric(const Rel& rel) {
		// TODO: make lookup table?
		switch (rel) {
			case EQ: return EQ;
			case MT: return MF;
			case MF: return MT;
			case GT: return GF;
			case GF: return GT;
			case BT: return BT;
		}
	}

	/**
	 * @brief Computes the symmetric set. This set contains the symmetric relation
	 *        for every element in the passed set.
	 */
	static RelSet symmetric(const RelSet& rs) {
		auto result = RelSet(rs);
		result.set(MT, contains(rs, MF));
		result.set(MF, contains(rs, MT));
		result.set(GT, contains(rs, GF));
		result.set(GF, contains(rs, GT));
		return result;
	}

	
	static std::ostream& operator<<(std::ostream& os, const Rel& rel) {
		switch(rel) {
			case EQ: os << "="; return os;
			case MT: os << "↦"; return os;
			case MF: os << "↤"; return os;
			case GT: os << "⇢"; return os;
			case GF: os << "⇠"; return os;
			case BT: os << "⋈"; return os;
			default: return os;
		}
	};

	static std::ostream& operator<<(std::ostream& os, const RelSet& rs) {
		if (rs.none()) os << "∅";
		else for (Rel rel : rs) os << rel;
		return os;
	};

}
