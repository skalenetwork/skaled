// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2929Test
 * @notice Tests cold vs warm storage/account access gas costs (EIP-2929).
 *
 * EIP-2929 spec: cold SLOAD = 2100 gas, warm SLOAD = 100 gas.
 * Pre-2929:      all SLOADs = 800 gas (no cold/warm distinction).
 */
contract EIP2929Test {
    mapping(uint256 => uint256) public data;

    constructor() {
        for (uint256 i = 0; i < 10; i++) {
            data[i] = i * 100;
        }
    }

    function readStorage(uint256 key) external view returns (uint256) {
        return data[key];
    }

    function writeStorage(uint256 key, uint256 value) external {
        data[key] = value;
    }

    function checkBalance(address addr) external view returns (uint256) {
        return addr.balance;
    }

    function checkCodeSize(address addr) external view returns (uint256 size) {
        assembly {
            size := extcodesize(addr)
        }
    }

    /**
     * @notice Measures gas for first (cold) and second (warm) SLOAD of the
     *         same key, plus a third read of a *different* key (cold again).
     */
    function measureColdVsWarmSload(uint256 key) external view returns (
        uint256 coldGas,
        uint256 warmGas,
        uint256 coldGas2
    ) {
        uint256 otherKey = key + 1;
        uint256 g0 = gasleft();
        uint256 _v1 = data[key];       // cold read of key
        uint256 g1 = gasleft();
        uint256 _v2 = data[key];       // warm read of key
        uint256 g2 = gasleft();
        uint256 _v3 = data[otherKey];  // cold read of different key
        uint256 g3 = gasleft();

        _v1; _v2; _v3;

        coldGas  = g0 - g1;
        warmGas  = g1 - g2;
        coldGas2 = g2 - g3;
    }

    /**
     * @notice Measures cold vs warm BALANCE access on an external address.
     *         EIP-2929: cold BALANCE = 2600, warm BALANCE = 100.
     */
    function measureColdVsWarmBalance(address addr) external view returns (
        uint256 coldGas,
        uint256 warmGas
    ) {
        uint256 g0 = gasleft();
        uint256 _b1 = addr.balance;
        uint256 g1 = gasleft();
        uint256 _b2 = addr.balance;
        uint256 g2 = gasleft();

        _b1; _b2;

        coldGas = g0 - g1;
        warmGas = g1 - g2;
    }
}
