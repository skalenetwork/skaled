// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

// Uncomment this line to use console.log
// import "hardhat/console.sol";



contract Lock {

    uint constant ARRAY_SIZE = 1000;
    uint[ARRAY_SIZE] balance;
    mapping(uint => uint) public writeMap;
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

        for (uint i = 0; i < balance.length; i++) {
            balance[i] = block.number;
        }
    }

    function withdraw() public {
        // Uncomment this line, and the import of "hardhat/console.sol", to print a log in your terminal
        // console.log("Unlock time is %o and block timestamp is %o", unlockTime, block.timestamp);


        require(msg.sender == owner, "You aren't the owner");

        emit Withdrawal(address(this).balance, block.timestamp);

        owner.transfer(address(this).balance);

        for (uint i = 0; i < balance.length; i++) {
            balance[i] = block.number;
        }

        for (uint i = 0; i < ARRAY_SIZE; i++) {
            writeMap[block.number * ARRAY_SIZE + i] = i;
        }

    }

    function blockNumber() external view returns (uint256) {
        return block.number;
    }
}