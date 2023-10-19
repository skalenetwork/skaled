// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract Push0 {

    uint256 public constant ZERO = 0;

    function getZero() public {
        // this triggers compiler using push0 to stack since operations use lots of zeros
        uint256 one = 0;
        one = one + 1  + ZERO;
        uint256 two = one * 0;
        uint256 three = one * ZERO;
    }
}