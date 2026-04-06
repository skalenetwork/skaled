// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// EIP-4399: PREVRANDAO opcode.
// Post-Paris, opcode 0x44 (formerly DIFFICULTY) returns the beacon RANDAO mix.
// In skaled (BFT, no beacon chain), prevRandao is hardcoded to 0.
contract EIP4399Test {
    function getPrevRandao() external view returns (uint256) {
        return block.prevrandao;
    }
}
