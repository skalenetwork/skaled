// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;


contract Tracer {

    event TopEvent(address indexed minter, uint256 amount, string topic);
    event InternalCallEvent(address indexed minter, uint256 amount, string topic);

    uint256 public balance;
    bool constructorVar = false;
    uint256 internalVar;

    event TopEvent(address indexed from, address indexed to, uint value);

    function die(address payable recipient) external {
        selfdestruct(recipient);
    }

    function mint(uint amount) public returns (uint256) {
        emit TopEvent(msg.sender, amount, "topic");
        balance += amount;
        internalCall(amount + 1);
        emit TopEvent(address(0), msg.sender, amount);
        return 1;
    }

    function internalCall(uint amount) public returns (uint256) {
        emit InternalCallEvent(msg.sender, amount, "internal topic");
        internalVar += amount;
        return 2;
    }


    constructor()  {
        require(!constructorVar, "Contract instance has already been initialized");
        constructorVar = true;
        mint(10000000000000000000000000000000000000000);
    }
}