// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract EIP3529Test {
    mapping(uint256 => uint256) public store;

    function prepopulate(uint256 key, uint256 value) external {
        store[key] = value;
    }

    function clearSlot(uint256 key) external returns (uint256 gasUsed) {
        uint256 g0 = gasleft();
        store[key] = 0;
        uint256 g1 = gasleft();
        gasUsed = g0 - g1;
    }

    function clearSlots(uint256[] calldata keys) external {
        for (uint256 i = 0; i < keys.length; i++) {
            store[keys[i]] = 0;
        }
    }

    function measureSelfdestructRefund() external returns (uint256 gasUsed) {
        SelfdestructTarget target = new SelfdestructTarget();
        uint256 g0 = gasleft();
        target.destroy(payable(address(this)));
        uint256 g1 = gasleft();
        gasUsed = g0 - g1;
    }
}

contract SelfdestructTarget {
    function destroy(address payable recipient) external {
        selfdestruct(recipient);
    }
}
