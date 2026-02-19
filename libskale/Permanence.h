//
// Created by stan on 23-10-2022.
//

#ifndef SKALED_PERMANENCE_H
#define SKALED_PERMANENCE_H
namespace skale {
enum class Permanence {
    Reverted,
    Committed,
    BlockCommitted,  ///< Committed at the end of the block
    Uncommitted,     ///< Uncommitted state for change log readings in tests.
    CommittedWithoutState
};

inline bool isStateCommitting( Permanence _p ) {
    return _p == Permanence::Committed || _p == Permanence::BlockCommitted;
}

}  // namespace skale
#endif  // SKALED_PERMANENCE_H
