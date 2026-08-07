// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract EIP3541Test {
    event DeployResult(bool success, address addr);

    function deployEFCode() external returns (bool success, address addr) {
        bytes memory initcode = hex"60EF60005360016000F3";
        assembly {
            addr := create(0, add(initcode, 0x20), mload(initcode))
            success := iszero(iszero(addr))
        }
        emit DeployResult(success, addr);
    }

    function deployFECode() external returns (bool success, address addr) {
        bytes memory initcode = hex"60FE60005360016000F3";
        assembly {
            addr := create(0, add(initcode, 0x20), mload(initcode))
            success := iszero(iszero(addr))
        }
        emit DeployResult(success, addr);
    }

    function deployEFCodeCreate2(bytes32 salt)
        external
        returns (bool success, address addr)
    {
        bytes memory initcode = hex"60EF60005360016000F3";
        assembly {
            addr := create2(0, add(initcode, 0x20), mload(initcode), salt)
            success := iszero(iszero(addr))
        }
        emit DeployResult(success, addr);
    }
}
