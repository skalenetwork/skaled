// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2929CreateRevertTest
 * @notice Verifies that a failed CREATE still warms the created address,
 *         and that addresses warmed only inside the failed initcode are
 *         rolled back to cold.
 */
contract EIP2929CreateRevertTest {

    event CreateRevertGasMeasured(
        uint256 createdAddrBalanceGas,
        uint256 innerWarmedAddrBalanceGas,
        uint256 coldRefAddrBalanceGas
    );

    /**
     * @notice Attempts a CREATE whose initcode warms `innerAddr` then reverts.
     *         After the failed CREATE, measures BALANCE gas on:
     *           1. The would-be-created address (expect warm, ~100)
     *           2. innerAddr that was warmed only inside initcode (expect cold, ~2600)
     *           3. coldRef that was never touched (expect cold, ~2600)
     *
     * @param innerAddr   Address accessed inside initcode (should revert to cold)
     * @param coldRef     Address never touched (cold baseline)
     */
    function measureCreateRevert(address innerAddr, address coldRef) external {
        // Build initcode that:
        //   1. BALANCEs innerAddr (warms it inside the child scope)
        //   2. REVERTs
        //
        // Bytecode layout:
        //   PUSH20 <innerAddr>   ;; 0x73 ++ 20-byte address
        //   BALANCE               ;; 0x31
        //   POP                   ;; 0x50
        //   PUSH1 0x00            ;; 0x60 0x00
        //   PUSH1 0x00            ;; 0x60 0x00
        //   REVERT                ;; 0xfd
        // Total: 26 bytes

        bytes memory initcode = abi.encodePacked(
            bytes1(0x73),       // PUSH20
            innerAddr,          // 20-byte address
            bytes1(0x31),       // BALANCE
            bytes1(0x50),       // POP
            bytes1(0x60),       // PUSH1
            bytes1(0x00),       //   0x00
            bytes1(0x60),       // PUSH1
            bytes1(0x00),       //   0x00
            bytes1(0xfd)        // REVERT
        );

        // Predict the created address (CREATE uses sender nonce).
        // We need to know it to measure BALANCE afterwards.
        address predicted = address(uint160(uint256(keccak256(
            abi.encodePacked(bytes1(0xd6), bytes1(0x94), address(this), bytes1(0x01))
        ))));

        // Attempt CREATE — it will revert inside initcode.
        address result;
        assembly {
            result := create(0, add(initcode, 32), mload(initcode))
        }
        // result should be address(0) since initcode reverted.

        // 1. BALANCE on the predicted created address — should be warm (~100).
        uint256 g0 = gasleft();
        uint256 _b0 = predicted.balance;
        uint256 g1 = gasleft();
        _b0;

        // 2. BALANCE on innerAddr — was warmed only inside reverted initcode,
        //    should be cold (~2600) because inner scope was rolled back.
        uint256 g2 = gasleft();
        uint256 _b1 = innerAddr.balance;
        uint256 g3 = gasleft();
        _b1;

        // 3. BALANCE on coldRef — never touched, cold baseline (~2600).
        uint256 g4 = gasleft();
        uint256 _b2 = coldRef.balance;
        uint256 g5 = gasleft();
        _b2;

        emit CreateRevertGasMeasured(g0 - g1, g2 - g3, g4 - g5);
    }
}
