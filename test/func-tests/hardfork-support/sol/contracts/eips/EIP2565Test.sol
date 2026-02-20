// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2565Test
 * @notice Tests the ModExp precompile at address 0x05 (EIP-2565 repricing).
 */
contract EIP2565Test {
    address constant MODEXP_PRECOMPILE = address(0x05);

    /**
     * @notice Raw modular exponentiation via the precompile.
     * @param base   Big-endian encoded base
     * @param exp    Big-endian encoded exponent
     * @param mod    Big-endian encoded modulus
     * @return result Big-endian encoded result
     */
    function modExp(
        bytes memory base,
        bytes memory exp,
        bytes memory mod
    ) public view returns (bytes memory result) {
        bytes memory input = abi.encodePacked(
            uint256(base.length),
            uint256(exp.length),
            uint256(mod.length),
            base,
            exp,
            mod
        );

        (bool success, bytes memory output) = MODEXP_PRECOMPILE.staticcall(input);
        require(success, "ModExp precompile call failed");
        result = output;
    }

    /**
     * @notice Convenience wrapper: computes (base ** exp) % mod for uint256 values.
     */
    function modExpUint(
        uint256 base,
        uint256 exp,
        uint256 mod
    ) external view returns (uint256) {
        bytes memory bBase = abi.encodePacked(base);
        bytes memory bExp = abi.encodePacked(exp);
        bytes memory bMod = abi.encodePacked(mod);

        bytes memory raw = modExp(bBase, bExp, bMod);
        require(raw.length == 32, "Unexpected output length");

        uint256 result;
        assembly {
            result := mload(add(raw, 32))
        }
        return result;
    }
}
