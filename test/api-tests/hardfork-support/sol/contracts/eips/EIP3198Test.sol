// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract EIP3198Test {
    function getBaseFee() external view returns (uint256) {
        return block.basefee;
    }

    function getBaseFeeWithBlockInfo()
        external
        view
        returns (uint256 baseFee, uint256 blockNum, uint256 timestamp)
    {
        baseFee = block.basefee;
        blockNum = block.number;
        timestamp = block.timestamp;
    }

    function rewardScalarFromBaseFee() external view returns (uint256) {
        uint256 currentBaseFee = block.basefee;
        uint256 rewardScalar = 1000 / currentBaseFee;
        return rewardScalar;
    }
}
