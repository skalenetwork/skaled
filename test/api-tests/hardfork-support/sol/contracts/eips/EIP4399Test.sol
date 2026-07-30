// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

// EIP-4399: PREVRANDAO opcode.
// Post-Paris, opcode 0x44 (formerly DIFFICULTY) returns the beacon RANDAO mix.
// In skaled, prevRandao = BLAKE3 of the previous block's BLS threshold signature,
// stored in the block header — non-zero for every block after block 1.
contract EIP4399Test {
    function getPrevRandao() external view returns (uint256) {
        return block.prevrandao;
    }
}
