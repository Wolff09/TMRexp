#pragma once

#include <vector>
#include "cfg.hpp"


namespace tmr {

	/**
	 * @brief Computes all shapes that result from executing the given command in the given shape.
	 * 
	 * @param cfg a configuration to compute post for
	 * @param tid 0-indexed id of thread executing ``stmt``
	 * @param msetup memory semantics to use (using ``PRF`` will result in checking for strong pointer races during execution)
	 * @return A list of pointers to shapes that result from thread ``tid`` executing ``stmt`` in an environment
	 *         described by ``shape``. All elements of that list are "fresh" and ownership must be claimed
	 *         (by calling this method the caller agrees to thoroughly care for deleting the passed objects).
	 */
	std::vector<Cfg> post(const Cfg& cfg, unsigned short tid);

	std::vector<Cfg> post(const Cfg& cfg, const Conditional& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Sequence& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Atomic& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const CompareAndSwap& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Break& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Malloc& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Assignment& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const NullAssignment& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const WriteRecData& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const SetRecEpoch& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const GetLocalEpochFromGlobalEpoch& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const InitRecPtr& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const IncrementGlobalEpoch& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const Killer& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const SetAddArg& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const SetAddSel& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const SetCombine& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const SetClear& stmt, unsigned short tid);
	std::vector<Cfg> post(const Cfg& cfg, const FreeAll& stmt, unsigned short tid);
}
