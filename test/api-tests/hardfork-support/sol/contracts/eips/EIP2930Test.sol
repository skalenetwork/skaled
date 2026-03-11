// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2930Test
 * @notice Tests access-list (Type 1) transactions – covers both EIP-2930
 *         and the EIP-2718 typed-transaction envelope.
 */
contract EIP2930Test {
    uint256 public slot0;
    uint256 public slot1;
    uint256 public slot2;
    uint256 public slot3;

    constructor() {
        slot0 = 10;
        slot1 = 20;
        slot2 = 30;
        slot3 = 40;
    }

    function readAllSlots() external view returns (uint256, uint256, uint256, uint256) {
        return (slot0, slot1, slot2, slot3);
    }

    function writeAllSlots(uint256 a, uint256 b, uint256 c, uint256 d) external {
        slot0 = a;
        slot1 = b;
        slot2 = c;
        slot3 = d;
    }
}
