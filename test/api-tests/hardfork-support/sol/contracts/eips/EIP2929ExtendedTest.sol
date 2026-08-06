// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2929ExtendedTest
 * @notice Tests additional EIP-2929 behaviours:
 *   1. Cold vs warm SSTORE (cold slot incurs 2100-gas surcharge).
 *   2. Cold vs warm EXTCODESIZE (cold = 2600, warm = 100).
 *   3. CREATE immediately warms the new contract address
 *      (subsequent BALANCE is warm ~100, not cold 2600).
 */
contract EIP2929ExtendedTest {
    // Slots used for SSTORE measurement.
    uint256 public storeSlot;   // slot 0
    uint256 public storeSlot1;  // slot 1

    /// Emitted by createAndMeasureBalance() with the measured gas values.
    event BalanceGasMeasured(
        address indexed createdAddr,
        uint256 warmBalanceGas,
        uint256 coldBalanceGas
    );

    // -----------------------------------------------------------------------
    // SSTORE cold vs warm
    // -----------------------------------------------------------------------

    /**
     * @notice Measures gas for SSTORE to a slot that was never read before
     *         (cold) versus a second write to the same slot (warm).
     *
     * EIP-2929 adds COLD_SLOAD_COST (2100) to the first access of a storage
     * slot by SSTORE.  The second access in the same transaction is warm and
     * does not pay this surcharge.
     *
     * @return coldGas  Gas consumed by the first (cold) SSTORE.
     * @return warmGas  Gas consumed by the second (warm) SSTORE.
     */
    function measureColdVsWarmSstore() external returns (
        uint256 coldGas,
        uint256 warmGas
    ) {
        uint256 g0 = gasleft();
        storeSlot = 1;          // cold SSTORE (first touch)
        uint256 g1 = gasleft();
        storeSlot = 2;          // warm SSTORE (same slot, already in access set)
        uint256 g2 = gasleft();

        coldGas = g0 - g1;
        warmGas = g1 - g2;
    }

    // -----------------------------------------------------------------------
    // EXTCODESIZE cold vs warm
    // -----------------------------------------------------------------------

    /**
     * @notice Measures gas for EXTCODESIZE on a cold address (first access)
     *         versus the same address accessed a second time (warm).
     *
     * EIP-2929: cold EXTCODESIZE = 2600 gas, warm = 100 gas.
     *
     * @param target  Any externally-supplied address (must not be pre-warmed).
     * @return coldGas  Gas consumed by first EXTCODESIZE on target.
     * @return warmGas  Gas consumed by second EXTCODESIZE on target.
     */
    function measureColdVsWarmExtcodesize(address target) external view returns (
        uint256 coldGas,
        uint256 warmGas
    ) {
        uint256 size;
        uint256 g0 = gasleft();
        assembly { size := extcodesize(target) }   // cold
        uint256 g1 = gasleft();
        assembly { size := extcodesize(target) }   // warm
        uint256 g2 = gasleft();
        size;

        coldGas = g0 - g1;
        warmGas = g1 - g2;
    }

    // -----------------------------------------------------------------------
    // CREATE warms the new address
    // -----------------------------------------------------------------------

    /**
     * @notice Deploys a minimal child contract via CREATE then immediately
     *         measures the BALANCE cost of the newly created address.
     *
     * Per EIP-2929: "When a CREATE opcode is executed, immediately add the
     * address being created to accessed_addresses."  So the very first BALANCE
     * query on the created address must be warm (~100 gas), not cold (2600).
     *
     * For comparison, we also measure BALANCE on a cold address (an address
     * that was never touched) to confirm the baseline cold cost.
     *
     * @param coldAddr  An address that has never been accessed in this tx.
     *
     * Emits {BalanceGasMeasured} with the results so the caller can read them
     * from the transaction receipt (works correctly in a real transaction even
     * when skaled runs eth_call in read-only mode, which would block CREATE).
     */
    function createAndMeasureBalance(address coldAddr) external {
        // Deploy the tiny child contract.
        bytes memory code = hex"60006000f3"; // PUSH1 0 PUSH1 0 RETURN
        address child;
        assembly {
            child := create(0, add(code, 32), mload(code))
        }
        require(child != address(0), "CREATE failed");

        // Measure BALANCE on the freshly created (warm) address.
        uint256 g0 = gasleft();
        uint256 _b1 = child.balance;
        uint256 g1 = gasleft();
        _b1;

        // Measure BALANCE on a cold address for comparison.
        uint256 g2 = gasleft();
        uint256 _b2 = coldAddr.balance;
        uint256 g3 = gasleft();
        _b2;

        emit BalanceGasMeasured(child, g0 - g1, g2 - g3);
    }
}
