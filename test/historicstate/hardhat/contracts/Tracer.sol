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

    function riskyFunction() public pure{
        revert("This function reverted!");
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


    error InsufficientBalance(uint256 requested, uint256 available);

    function riskyFunction() public pure {
        revert("This function reverted!");
    }


    function riskyFunction2() public pure {
        revert InsufficientBalance(1, 1);
    }


    function mint(uint amount) public returns (uint256) {
        emit InternalCallEvent(msg.sender, amount, "topic");
        balance += amount;
        internalCall(amount + 1);
        precompileCall();
        secondContract.mint(amount);
        return 1;
    }

    function mint2(uint amount) public returns (uint256) {
        emit InternalCallEvent(msg.sender, amount, "topic");
        secondContract = new SecondContract();
        balance += amount;
        internalCall(amount + 1);
        precompileCall();
        secondContract.mint(amount);
        return 1;
    }


    event ErrorHandled(string message);

    // test revert
    function readableRevert(uint) public returns (uint256) {

        try  this.riskyFunction() {
            // If the call succeeds, this block is executed
        } catch Error(string memory reason) {
            // If the call reverts with an error message, this block is executed
            emit ErrorHandled(reason);
        } catch (bytes memory) {
            // If the call reverts without an error message, this block is executed
            emit ErrorHandled("External call failed without an error message");
        }

        try  this.riskyFunction2() {
            // If the call succeeds, this block is executed
        } catch Error(string memory reason) {
            // If the call reverts with an error message, this block is executed
            emit ErrorHandled(reason);
        } catch (bytes memory ) {
            // If the call reverts without an error message, this block is executed
            emit ErrorHandled("External call failed without an error message");
        }


        require(false, "INSUFFICIENT BALANCE");
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

    function getBalance() external view returns (uint256) {
        return balance;
    }
}