// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2929RevertTest
 * @notice Tests EIP-2929 access-set behaviour across sub-call reverts.
 *
 * EIP-2929 / go-ethereum semantics: when a sub-call scope reverts,
 * accessed_addresses and accessed_storage_keys are restored to the state
 * they were in before that scope was entered.  A storage slot warmed only
 * inside a reverted sub-call must therefore cost cold gas in the outer frame.
 */
contract EIP2929RevertTest {
    uint256 public slot0 = 42;
    uint256 public slot1 = 43;

    /// @dev Reads slot0 then reverts. Called via try/catch to create a
    ///      reverted sub-call scope that warms slot0.
    function readSlot0ThenRevert() external view {
        uint256 v = slot0;
        v;
        revert("intentional");
    }

    /**
     * @notice Triggers a sub-call that warms slot0 then reverts, then
     *         measures the SLOAD cost of slot0 in the outer frame.
     *
     * If access sets are restored on revert (correct per EIP-2929):
     *   gasAfterRevert ~= 2100  (cold)
     * If access sets persist across the revert (incorrect):
     *   gasAfterRevert ~= 100   (warm)
     *
     * gasSlot1 is a cold reference: slot1 is never accessed anywhere.
     *
     * @return gasAfterRevert  Gas consumed by SLOAD(slot0) in the outer frame
     *                         after the reverted sub-call that warmed it.
     * @return gasSlot1        Gas consumed by SLOAD(slot1) — cold reference.
     */
    function measureSlotCostAfterRevert() external view returns (
        uint256 gasAfterRevert,
        uint256 gasSlot1
    ) {
        try this.readSlot0ThenRevert() {} catch {}

        uint256 g0 = gasleft();
        uint256 v0 = slot0;
        uint256 g1 = gasleft();
        v0;

        uint256 g2 = gasleft();
        uint256 v1 = slot1;
        uint256 g3 = gasleft();
        v1;

        gasAfterRevert = g0 - g1;
        gasSlot1       = g2 - g3;
    }

    /**
     * @notice Returns the gas cost of a warm SLOAD (same slot accessed twice,
     *         no revert).  Used as the warm-cost reference.
     */
    function measureWarmSload() external view returns (uint256 warmGas) {
        uint256 _first = slot0;
        _first;
        uint256 g0 = gasleft();
        uint256 _second = slot0;
        uint256 g1 = gasleft();
        _second;
        warmGas = g0 - g1;
    }
}
