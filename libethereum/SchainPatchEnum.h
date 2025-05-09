#ifndef SCHAINPATCHENUM_H
#define SCHAINPATCHENUM_H

#include <stdexcept>
#include <string>

enum class SchainPatchEnum {
    RevertableFSPatch,
    PrecompiledConfigPatch,
    PowCheckPatch,
    CorrectForkInPowPatch,
    ContractStorageZeroValuePatch,
    PushZeroPatch,
    ContractStoragePatch,
    StorageDestructionPatch,
    SkipInvalidTransactionsPatch,
    VerifyDaSigsPatch,
    FastConsensusPatch,
    EIP1559TransactionsPatch,
    VerifyBlsSyncPatch,
    FlexibleDeploymentPatch,
    ExternalGasPatch,
    ClearPartialReceiptsPatch,
    InvalidTransactionFormatPatch,
    PatchesCount
};

#ifdef MIRAGE
std::unordered_set< SchainPatchEnum > preEnabledForMIRAGE = { CorrectForkInPowPatch,
    ContractStorageZeroValuePatch, PushZeroPatch, ContractStoragePatch, StorageDestructionPatch,
    SkipInvalidTransactionsPatch, VerifyDaSigsPatch, FastConsensusPatch, EIP1559TransactionsPatch,
    VerifyBlsSyncPatch, ClearPartialReceiptsPatch, InvalidTransactionFormatPatch };
std::unordered_set< SchainPatchEnum > preDisabledForMIRAGE = { RevertableFSPatch, PowCheckPatch,
    CorrectForkInPowPatch, FlexibleDeploymentPatch, ExternalGasPatch };
#endif

extern SchainPatchEnum getEnumForPatchName( const std::string& _patchName );
extern std::string getPatchNameForEnum( SchainPatchEnum enumValue );


#endif  // SCHAINPATCHENUM_H
