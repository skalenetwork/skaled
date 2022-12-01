// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

// Uncomment this line to use console.log
// import "hardhat/console.sol";



contract Lock {

    uint constant ARRAY_SIZE = 1000;
    uint256[ARRAY_SIZE] balance;
    uint256 counter = 1;

    mapping(uint256 => uint256) public writeMap;
    uint public unlockTime;
    address payable public owner;

    event Withdrawal(uint amount, uint when);

    constructor(uint _unlockTime) payable {
        require(
            block.timestamp < _unlockTime,
            "Unlock time should be in the future"
        );

        unlockTime = _unlockTime;
        owner = payable(msg.sender);

    }

    function store() public {
        // Uncomment this line, and the import of "hardhat/console.sol", to print a log in your terminal
        // console.log("Unlock time is %o and block timestamp is %o", unlockTime, block.timestamp);


        for (uint256 i = 0; i < ARRAY_SIZE; i++) {
            counter++;
            writeMap[counter] = 153453455467547588686 + block.timestamp + counter;
        }

    }

    function blockNumber() external view returns (uint256) {
        return block.number;
    }
}