// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.9;

import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";




contract Lock is Initializable {
    uint public totalSupply;
    mapping(address => uint) public balanceOf;
    mapping(address => mapping(address => uint)) public allowance;
    string public name;
    string public symbol;
    uint8 public decimals;
    bool private initialized;

    uint constant ARRAY_SIZE = 1000;
    uint256[ARRAY_SIZE] balance;
    uint256 counter;
    mapping(uint256 => uint256) public writeMap;
    uint public unlockTime;
    address public owner;

    event Transfer(address indexed from, address indexed to, uint value);
    event Approval(address indexed owner, address indexed spender, uint value);


    function transfer(address recipient, uint amount) external returns (bool) {
        balanceOf[msg.sender] -= amount;
        balanceOf[recipient] += amount;
        emit Transfer(msg.sender, recipient, amount);
        return true;
    }

    function approve(address spender, uint amount) external returns (bool) {
        allowance[msg.sender][spender] = amount;
        emit Approval(msg.sender, spender, amount);
        return true;
    }

    function transferFrom(
        address sender,
        address recipient,
        uint amount
    ) external returns (bool) {
      //  allowance[sender][msg.sender] -= amount;
      //  balanceOf[sender] -= amount;
     //   balanceOf[recipient] += amount;
      //  emit Transfer(sender, recipient, amount);
        return true;
    }

    function mint(uint amount) public {
        balanceOf[msg.sender] += amount;
        totalSupply += amount;
        emit Transfer(address(0), msg.sender, amount);
    }

    function burn(uint amount) external {
        balanceOf[msg.sender] -= amount;
        totalSupply -= amount;
        emit Transfer(msg.sender, address(0), amount);
    }





    function  initialize() public {
      require(!initialized, "Contract instance has already been initialized");
      initialized = true;

        name = "Lock";
        symbol = "LOCK";
        decimals = 18;
        owner = msg.sender;
        counter = 1;


        mint(10000000000000000000000000000000000000000);


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