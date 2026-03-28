#include "SchainPatch.h"

#include <libethcore/ChainOperationParams.h>

using namespace dev::eth;

ChainOperationParams SchainPatch::chainParams;
std::atomic< time_t > SchainPatch::committedBlockTimestamp;

SchainPatchEnum getEnumForPatchName( const std::string& _patchName ) {
    if ( _patchName == "RevertableFSPatch" )
        return SchainPatchEnum::RevertableFSPatch;
    else if ( _patchName == "PrecompiledConfigPatch" )
        return SchainPatchEnum::PrecompiledConfigPatch;
    else if ( _patchName == "PowCheckPatch" )
        return SchainPatchEnum::PowCheckPatch;
    else if ( _patchName == "CorrectForkInPowPatch" )
        return SchainPatchEnum::CorrectForkInPowPatch;
    else if ( _patchName == "ContractStorageZeroValuePatch" )
        return SchainPatchEnum::ContractStorageZeroValuePatch;
    else if ( _patchName == "PushZeroPatch" )
        return SchainPatchEnum::PushZeroPatch;
    else if ( _patchName == "ContractStoragePatch" )
        return SchainPatchEnum::ContractStoragePatch;
    else if ( _patchName == "StorageDestructionPatch" )
        return SchainPatchEnum::StorageDestructionPatch;
    else if ( _patchName == "SkipInvalidTransactionsPatch" )
        return SchainPatchEnum::SkipInvalidTransactionsPatch;
    else if ( _patchName == "VerifyDaSigsPatch" )
        return SchainPatchEnum::VerifyDaSigsPatch;
    else if ( _patchName == "FastConsensusPatch" )
        return SchainPatchEnum::FastConsensusPatch;
    else if ( _patchName == "EIP1559TransactionsPatch" )
        return SchainPatchEnum::EIP1559TransactionsPatch;
    else if ( _patchName == "VerifyBlsSyncPatch" )
        return SchainPatchEnum::VerifyBlsSyncPatch;
    else if ( _patchName == "FlexibleDeploymentPatch" )
        return SchainPatchEnum::FlexibleDeploymentPatch;
    else if ( _patchName == "ExternalGasPatch" )
        return SchainPatchEnum::ExternalGasPatch;
    else if ( _patchName == "ClearPartialReceiptsPatch" )
        return SchainPatchEnum::ClearPartialReceiptsPatch;
    else if ( _patchName == "SingleStateCommitPerBlockPatch" )
        return SchainPatchEnum::SingleStateCommitPerBlockPatch;
    else if ( _patchName == "InvalidTransactionFormatPatch" )
        return SchainPatchEnum::InvalidTransactionFormatPatch;
    else if ( _patchName == "CurrentBlockRandomPatch" )
        return SchainPatchEnum::CurrentBlockRandomPatch;
    else if ( _patchName == "GroupIndexInitPatch" )
        return SchainPatchEnum::GroupIndexInitPatch;
    else if ( _patchName == "ContractCreationReadOnlyPatch" )
        return SchainPatchEnum::ContractCreationReadOnlyPatch;
#ifdef BITE
    else if ( _patchName == "BITE2Patch" || _patchName == "Bite2Patch" )
        return SchainPatchEnum::Bite2Patch;
#endif  // BITE
#ifdef FAIR
    else if ( _patchName == "DisableSelfDestructPatch" )
        return SchainPatchEnum::DisableSelfDestructPatch;
#endif
    else
        throw std::out_of_range( _patchName );
}

std::string getPatchNameForEnum( SchainPatchEnum _enumValue ) {
    switch ( _enumValue ) {
    case SchainPatchEnum::RevertableFSPatch:
        return "RevertableFSPatch";
    case SchainPatchEnum::PrecompiledConfigPatch:
        return "PrecompiledConfigPatch";
    case SchainPatchEnum::PowCheckPatch:
        return "PowCheckPatch";
    case SchainPatchEnum::CorrectForkInPowPatch:
        return "CorrectForkInPowPatch";
    case SchainPatchEnum::ContractStorageZeroValuePatch:
        return "ContractStorageZeroValuePatch";
    case SchainPatchEnum::PushZeroPatch:
        return "PushZeroPatch";
    case SchainPatchEnum::ContractStoragePatch:
        return "ContractStoragePatch";
    case SchainPatchEnum::StorageDestructionPatch:
        return "StorageDestructionPatch";
    case SchainPatchEnum::SkipInvalidTransactionsPatch:
        return "SkipInvalidTransactionsPatch";
    case SchainPatchEnum::VerifyDaSigsPatch:
        return "VerifyDaSigsPatch";
    case SchainPatchEnum::FastConsensusPatch:
        return "FastConsensusPatch";
    case SchainPatchEnum::EIP1559TransactionsPatch:
        return "EIP1559TransactionsPatch";
    case SchainPatchEnum::VerifyBlsSyncPatch:
        return "VerifyBlsSyncPatch";
    case SchainPatchEnum::FlexibleDeploymentPatch:
        return "FlexibleDeploymentPatch";
    case SchainPatchEnum::ExternalGasPatch:
        return "ExternalGasPatch";
    case SchainPatchEnum::ClearPartialReceiptsPatch:
        return "ClearPartialReceiptsPatch";
    case SchainPatchEnum::SingleStateCommitPerBlockPatch:
        return "SingleStateCommitPerBlockPatch";
    case SchainPatchEnum::InvalidTransactionFormatPatch:
        return "InvalidTransactionFormatPatch";
    case SchainPatchEnum::CurrentBlockRandomPatch:
        return "CurrentBlockRandomPatch";
    case SchainPatchEnum::GroupIndexInitPatch:
        return "GroupIndexInitPatch";
    case SchainPatchEnum::ContractCreationReadOnlyPatch:
        return "ContractCreationReadOnlyPatch";
#ifdef BITE
    case SchainPatchEnum::Bite2Patch:
        return "Bite2Patch";
#endif  // BITE
#ifdef FAIR
    case SchainPatchEnum::DisableSelfDestructPatch:
        return "DisableSelfDestructPatch";
#endif
    default:
        throw std::out_of_range(
            "UnknownPatch #" + std::to_string( static_cast< size_t >( _enumValue ) ) );
    }
}

#ifdef FAIR
const std::unordered_set< SchainPatchEnum > SchainPatch::preEnabledForFAIR = {
    SchainPatchEnum::PowCheckPatch, SchainPatchEnum::CorrectForkInPowPatch,
    SchainPatchEnum::ContractStorageZeroValuePatch, SchainPatchEnum::PushZeroPatch,
    SchainPatchEnum::ContractStoragePatch, SchainPatchEnum::StorageDestructionPatch,
    SchainPatchEnum::SkipInvalidTransactionsPatch, SchainPatchEnum::VerifyDaSigsPatch,
    SchainPatchEnum::FastConsensusPatch, SchainPatchEnum::EIP1559TransactionsPatch,
    SchainPatchEnum::VerifyBlsSyncPatch, SchainPatchEnum::ClearPartialReceiptsPatch,
    SchainPatchEnum::InvalidTransactionFormatPatch, SchainPatchEnum::CurrentBlockRandomPatch,
    SchainPatchEnum::GroupIndexInitPatch
};
const std::unordered_set< SchainPatchEnum > SchainPatch::preDisabledForFAIR = {
    SchainPatchEnum::RevertableFSPatch, SchainPatchEnum::FlexibleDeploymentPatch,
    SchainPatchEnum::ExternalGasPatch
};
#endif

void SchainPatch::init( const dev::eth::ChainOperationParams& _cp ) {
    chainParams = _cp;
    for ( size_t i = 0; i < static_cast< size_t >( SchainPatchEnum::PatchesCount ); ++i ) {
        printInfo( getPatchNameForEnum( static_cast< SchainPatchEnum >( i ) ),
            _cp.getPatchTimestamp( static_cast< SchainPatchEnum >( i ) ) );
    }
}

void SchainPatch::useLatestBlockTimestamp( time_t _timestamp ) {
    committedBlockTimestamp = _timestamp;
}

void SchainPatch::printInfo( const std::string& _patchName, time_t _timeStamp ) {
#ifdef FAIR
    if ( preEnabledForFAIR.count( getEnumForPatchName( _patchName ) ) > 0 ) {
        cnote << "Patch " << _patchName << " is enabled";
        return;
    }
    if ( preDisabledForFAIR.count( getEnumForPatchName( _patchName ) ) > 0 ) {
        cnote << "Patch " << _patchName << " is disabled";
        return;
    }
#endif
    if ( _timeStamp == 0 ) {
        cnote << "Patch " << _patchName << " is disabled";
    } else {
        cnote << "Patch " << _patchName << " is set at timestamp " << _timeStamp;
    }
}

bool SchainPatch::isPatchEnabledWhen(
    SchainPatchEnum _patchEnum, time_t _committedBlockTimestamp ) {
#ifdef FAIR
    if ( preEnabledForFAIR.count( _patchEnum ) > 0 )
        return true;
    if ( preDisabledForFAIR.count( _patchEnum ) > 0 )
        return false;
#endif
    time_t activationTimestamp = chainParams.getPatchTimestamp( _patchEnum );
    return activationTimestamp != 0 && _committedBlockTimestamp >= activationTimestamp;
}

EVMSchedule PushZeroPatch::makeSchedule( const EVMSchedule& _base ) {
    EVMSchedule ret = _base;
    ret.havePush0 = true;
    return ret;
}
