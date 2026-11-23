#ifndef SCHAINPATCH_H
#define SCHAINPATCH_H

#include "SchainPatchEnum.h"

#include <libethcore/ChainOperationParams.h>

#include <libdevcore/Log.h>

#include <string>

#ifdef FAIR
#include <unordered_set>
#endif

namespace dev {
namespace eth {
struct EVMSchedule;
}
}  // namespace dev


class SchainPatch {
public:
    static void init( const dev::eth::ChainOperationParams& _cp );
    static void useLatestBlockTimestamp( time_t _timestamp );

protected:
    static void printInfo( const std::string& _patchName, time_t _timeStamp );
    static bool isPatchEnabledInWorkingBlock( SchainPatchEnum _patchEnum ) {
#ifdef FAIR
        if ( preEnabledForFAIR.count( _patchEnum ) > 0 )
            return true;
        if ( preDisabledForFAIR.count( _patchEnum ) > 0 )
            return false;
#endif
        time_t activationTimestamp = chainParams.getPatchTimestamp( _patchEnum );
        return activationTimestamp != 0 && committedBlockTimestamp >= activationTimestamp;
    }
    static bool isPatchEnabledWhen( SchainPatchEnum _patchEnum, time_t _committedBlockTimestamp );

protected:
    static dev::eth::ChainOperationParams chainParams;
    static std::atomic< time_t > committedBlockTimestamp;
#ifdef FAIR
    static const std::unordered_set< SchainPatchEnum > preEnabledForFAIR;
    static const std::unordered_set< SchainPatchEnum > preDisabledForFAIR;
#endif
};


#define DEFINE_AMNESIC_PATCH( CustomPatch )                                       \
    class CustomPatch : public SchainPatch {                                      \
    public:                                                                       \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; } \
        static bool isEnabledInWorkingBlock() {                                   \
            return isPatchEnabledInWorkingBlock( getEnum() );                     \
        }                                                                         \
    };

// TODO One more overload - with EnvInfo?
#define DEFINE_SIMPLE_PATCH( CustomPatch )                                        \
    class CustomPatch : public SchainPatch {                                      \
    public:                                                                       \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; } \
        static bool isEnabledInWorkingBlock() {                                   \
            return isPatchEnabledInWorkingBlock( getEnum() );                     \
        }                                                                         \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {            \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );     \
        }                                                                         \
    };

#define DEFINE_EVM_PATCH( CustomPatch )                                                 \
    class CustomPatch : public SchainPatch {                                            \
    public:                                                                             \
        static SchainPatchEnum getEnum() { return SchainPatchEnum::CustomPatch; }       \
        static bool isEnabledInWorkingBlock() {                                         \
            return isPatchEnabledInWorkingBlock( getEnum() );                           \
        }                                                                               \
        static bool isEnabledWhen( time_t _committedBlockTimestamp ) {                  \
            return isPatchEnabledWhen( getEnum(), _committedBlockTimestamp );           \
        }                                                                               \
        static dev::eth::EVMSchedule makeSchedule( const dev::eth::EVMSchedule& base ); \
    };

/*
 * Context: enable revertable filestorage precompileds
 */
DEFINE_SIMPLE_PATCH( RevertableFSPatch )

/*
 * Context: enable precompiled contracts to read historical config data
 */
DEFINE_AMNESIC_PATCH( PrecompiledConfigPatch )

/*
 * Context: enable fix for POW txns gas limit check
 */
DEFINE_SIMPLE_PATCH( PowCheckPatch )

/*
 * Context: use current, and not Constantinople,  fork in Transaction::checkOutExternalGas()
 */
DEFINE_SIMPLE_PATCH( CorrectForkInPowPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_AMNESIC_PATCH( ContractStorageZeroValuePatch )

/*
 * Context: enable effective storage destruction
 */
DEFINE_EVM_PATCH( PushZeroPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_SIMPLE_PATCH( VerifyDaSigsPatch )

/*
 * Context: contractStorageUsed counter didn't work well in one case
 * Solution: we fixed the bug and added new config field introudceChangesTimestamp
 * Purpose: avoid incorrect txn behaviour
 * Version introduced:
 */
DEFINE_AMNESIC_PATCH( ContractStoragePatch )

/*
 * Context: enable effective storage destruction
 */
DEFINE_AMNESIC_PATCH( StorageDestructionPatch );

/*
 * Enable Berlin fork related changes (EIP-2718, EIP-2930, EIP-2929, EIP-2565)
 */
DEFINE_EVM_PATCH( BerlinForkPatch );

/*
 * Enable restriction on contract storage size, when it's doing selfdestruct
 */
DEFINE_SIMPLE_PATCH( EIP1559TransactionsPatch );

/*
 * Enable bls signatures verification for sync node
 */
DEFINE_AMNESIC_PATCH( VerifyBlsSyncPatch );

/*
 * Purpose: passing both transaction origin and sender to the ConfigController contract
 * Version introduced: 3.19.0
 */
DEFINE_SIMPLE_PATCH( FlexibleDeploymentPatch );

/*
 * Context: fix externalGas calculation
 */
DEFINE_SIMPLE_PATCH( ExternalGasPatch );

/*
 * Purpose: do not save partial receipts after block is executed
 * Version introduced: 4.0.0
 */
DEFINE_SIMPLE_PATCH( ClearPartialReceiptsPatch );

/*
 * Context: fix the check in transaction constructor
 * maxFeePerGas cannot be less than maxPriorityFeePerGas
 */
DEFINE_SIMPLE_PATCH( InvalidTransactionFormatPatch );

/*
 * Purpose: using current block in getBlockRandom
 * Version introduced: 4.1.0
 */
DEFINE_SIMPLE_PATCH( CurrentBlockRandomPatch );

/*
 * Context: fix group index initialization on startup
 * if skaled exits after the first block after rotation timestamp
 * then it starts again and initializes with wrong group index
 * Version introduced: 4.1.0
 */
DEFINE_AMNESIC_PATCH( GroupIndexInitPatch );

/*
 * Enable London fork changes (EIP-1559 baseFee, EIP-3198 BASEFEE opcode,
 * EIP-3529 reduced refunds, EIP-3541 reject 0xEF contracts)
 */
DEFINE_EVM_PATCH( LondonForkPatch );

#ifdef BITE
/*
 * Purpose: gate BITE2 features (CTX precompileds and CTX transaction detection)
 * behind a timestamp-controlled patch so nodes activate them synchronously.
 * When not enabled: BITE2 precompileds (submitCTX, encryptTE, encryptECIES)
 * return a revert and CTX status is not set in TransactionBase constructor.
 */
DEFINE_SIMPLE_PATCH( Bite2Patch );

/*
 * DecryptedTxFieldsPatch and DecryptedTxFieldsLegacyStartPatch work as a pair. They choose how
 * decrypted fields are matched to the BITE transactions of a block. Two behaviours exist,
 * both in libethereum/patches/DecryptedTxFieldsPatch.cpp:
 *   - current: each transaction gets the fields stored under its own index in the block;
 *   - legacy:  the matching used by releases 5.1.0 - 5.3.x, kept only so that blocks
 *              executed by those releases re-execute to the same result.
 *
 * Which one a block uses depends on the timestamp of its parent block, where S is the
 * DecryptedTxFieldsLegacyStartPatch timestamp and F the DecryptedTxFieldsPatch timestamp:
 *
 *   parent timestamp:   before S   |   from S, before F   |   from F
 *   behaviour:          current    |        legacy        |   current
 *
 * DecryptedTxFieldsPatch (decryptedTxFieldsPatchTimestamp)
 *   Purpose: end the legacy period. Set it to a future time once every node of the chain
 *   runs a release that has this patch, like any other patch.
 *   When not set: the legacy period does not end.
 *   Version introduced: 5.3.1
 */
DEFINE_SIMPLE_PATCH( DecryptedTxFieldsPatch );

/*
 * DecryptedTxFieldsLegacyStartPatch (decryptedTxFieldsLegacyStartPatchTimestamp)
 *   Purpose: say when the legacy period started on this chain, i.e. when the chain moved to
 *   release 5.1.0 or later. It describes the past and is only needed to re-execute blocks
 *   that are older than that. It must be a past timestamp and the same on every node.
 *   When not set (0): the legacy period starts at the first block. This is what releases
 *   without this patch do, and it is right for a chain that never ran a release before 5.1.0.
 *   Note that 0 does NOT mean "disabled" here, unlike other patches.
 *   Version introduced: 5.3.1
 */
class DecryptedTxFieldsLegacyStartPatch : public SchainPatch {
public:
    static SchainPatchEnum getEnum() { return SchainPatchEnum::DecryptedTxFieldsLegacyStartPatch; }
    static bool isReachedWhen( time_t _committedBlockTimestamp ) {
        time_t startTimestamp = chainParams.getPatchTimestamp( getEnum() );
        return startTimestamp == 0 || _committedBlockTimestamp >= startTimestamp;
    }
};
#endif  // BITE

/*
 * Purpose: enable state mode so the database commit is executed only once per block.
 */
DEFINE_SIMPLE_PATCH( SingleStateCommitPerBlockPatch );

DEFINE_SIMPLE_PATCH( ContractCreationReadOnlyPatch );

/*
 * Paris fork (EIP-3675 + EIP-4399): difficulty=0, no uncles, and the header carries a
 * RANDAO-style accumulator: prevRandao(N) = prevRandao(N-1) XOR BLAKE3(thresholdSig(N-1)),
 * seeded by the zero of pre-Paris parents. Each term is the previous block's consensus
 * threshold signature hashed — the same derivation the getBlockRandom precompiled uses —
 * and the parent's mix is read from its stored header, so committee rotation adds
 * cross-epoch lookahead protection. The value is derived once at block construction
 * (SkaleHost::createBlock) and only ever read from the header afterwards (PREVRANDAO
 * opcode, replay, historic queries). Genesis and block 1 keep zero. Ethash::verify pins
 * the header shape (2 seal fields, nonce=0) but cannot re-derive the value; changing the
 * derivation after this patch has shipped requires a NEW patch.
 */
DEFINE_SIMPLE_PATCH( ParisForkPatch );

#ifdef FAIR
DEFINE_SIMPLE_PATCH( DisableSelfDestructPatch );
#endif
#endif  // SCHAINPATCH_H
