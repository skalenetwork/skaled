// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

contract SecondContract {


    event InternalCallEvent(address indexed minter, uint256 amount, string topic);

    uint256 public balance;
    bool constructorVar = false;
    uint256 internalVar;


    function die(address payable recipient) external {
        selfdestruct(recipient);
    }

    function mint(uint amount) public returns (uint256) {
        emit InternalCallEvent(msg.sender, amount, "topic");
        balance += amount;
        internalCall(amount + 1);
        precompileCall();
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

    function precompileCall() public pure returns (bytes32) {
        string memory input = "Hello, world!";
        bytes32 expectedHash = keccak256(bytes(input));
        return expectedHash;
    }
}

contract Tracer {

    SecondContract secondContract;

    event InternalCallEvent(address indexed minter, uint256 amount, string topic);

    uint256 public balance;
    bool constructorVar = false;
    uint256 internalVar;


    function die(address payable recipient) external {
        selfdestruct(recipient);
    }

    function mint(uint amount) public returns (uint256) {
        emit InternalCallEvent(msg.sender, amount, "topic");
        balance += amount;
        internalCall(amount + 1);
        precompileCall();
        secondContract.mint(amount);
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
        //mint(10000000000000000000000000000000000000000);
    }

    function precompileCall() public pure returns (bytes32) {
        string memory input = "Hello, world!";
        bytes32 expectedHash = keccak256(bytes(input));
        return expectedHash;
    }

    function blockNumber() external view returns (uint256) {
        return block.number;
    }
}