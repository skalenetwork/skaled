//
// Created by stan on 23-10-2022.
//

#ifndef SKALED_PERMANENCE_H
#define SKALED_PERMANENCE_H
namespace skale {
enum class Permanence {
    Reverted,
    Committed,
    Uncommitted,  ///< Uncommitted state for change log readings in tests.
    CommittedWithoutState
};
}
#endif  // SKALED_PERMANENCE_H
