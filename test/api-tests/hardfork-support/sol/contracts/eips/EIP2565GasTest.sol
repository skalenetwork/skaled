// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2565GasTest
 * @notice Tests EIP-2565 ModExp gas formula correctness.
 *
 * EIP-2565 formula:
 *   max(200, floor(words² × iterationCount / 3))
 * where:
 *   words          = ceil(max(base_len, mod_len) / 8)
 *   iterationCount = f(exp_len, exp_value)  (see EIP spec)
 *
 * Tests:
 *   1. measureModexpGas: calls the precompile with controlled byte-length
 *      inputs and returns the gas consumed, so the caller can compare
 *      against the expected formula result.
 *   2. measureZeroExpGas: calls the precompile with exponent = 0 so
 *      iterationCount = 0, which means the formula yields 0 and the result
 *      is clamped to the 200-gas floor.
 */
contract EIP2565GasTest {
    address constant MODEXP = address(0x05);

    /**
     * @notice Calls ModExp with byte-level control over base/exp/mod lengths
     *         and returns gas consumed by the precompile call.
     *
     * @param base      Raw bytes for the base (length controls gas).
     * @param exp       Raw bytes for the exponent.
     * @param mod       Raw bytes for the modulus (must be non-zero).
     * @return gasUsed  Gas consumed by the staticcall to the precompile.
     * @return result   Raw output bytes from the precompile.
     */
    function measureModexpGas(
        bytes memory base,
        bytes memory exp,
        bytes memory mod
    ) external view returns (uint256 gasUsed, bytes memory result) {
        bytes memory input = abi.encodePacked(
            uint256(base.length),
            uint256(exp.length),
            uint256(mod.length),
            base,
            exp,
            mod
        );

        uint256 g0 = gasleft();
        bool ok;
        (ok, result) = MODEXP.staticcall(input);
        uint256 g1 = gasleft();

        require(ok, "ModExp call failed");
        gasUsed = g0 - g1;
    }

    /**
     * @notice Calls ModExp with exponent = 0 (i.e. base^0 mod m = 1).
     *         iterationCount = 0 for an exp value of 0, so the EIP-2565
     *         formula gives 0, and the result is clamped to the 200-gas floor.
     *
     * Uses 32-byte base and modulus (max_length = 32, words = 4, words² = 16).
     * Expected precompile gas = max(200, 16 * 0 / 3) = 200.
     *
     * @return gasUsed  Gas consumed by the precompile call.
     * @return value    Result of (base^0 mod m), expected = 1.
     */
    function measureZeroExpGas(
        uint256 base,
        uint256 mod
    ) external view returns (uint256 gasUsed, uint256 value) {
        bytes memory bBase = abi.encodePacked(base);   // 32 bytes
        bytes memory bExp  = abi.encodePacked(uint256(0)); // 32 bytes, value 0
        bytes memory bMod  = abi.encodePacked(mod);    // 32 bytes

        bytes memory input = abi.encodePacked(
            uint256(32), uint256(32), uint256(32),
            bBase, bExp, bMod
        );

        uint256 g0 = gasleft();
        bool ok;
        bytes memory raw;
        (ok, raw) = MODEXP.staticcall(input);
        uint256 g1 = gasleft();

        require(ok, "ModExp call failed");
        require(raw.length == 32, "Unexpected output length");
        assembly { value := mload(add(raw, 32)) }
        gasUsed = g0 - g1;
    }
}
