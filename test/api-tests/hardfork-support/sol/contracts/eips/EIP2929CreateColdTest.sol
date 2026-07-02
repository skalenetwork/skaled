// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/**
 * @title EIP2929CreateColdTest
 * @notice Demonstrates the EIP-2929 / EIP-2681 ordering guarantee that the
 *         nonce-overflow fix relies on: a CREATE that is *aborted before* the
 *         EVM warms the would-be contract address leaves that address COLD,
 *         while a CREATE that actually executes leaves it WARM.
 *
 *         The exact EIP-2681 trigger (sender nonce == 2^64-1) cannot be reached
 *         by normal RPC transactions — there is no setNonce. The execution-specs
 *         workload covers it with genesis-preseeded stubs, while this contract
 *         exercises the closest normal-RPC abort path: a CREATE
 *         whose endowment exceeds the creator's balance, which (like the nonce
 *         overflow) aborts before the address is added to the access set, so the
 *         creator's nonce is not consumed and the would-be address stays cold.
 */
contract EIP2929CreateColdTest {
    event CreateColdGasMeasured(
        address measuredAddr,
        uint256 abortedCreateBalanceGas,  // expect cold (~2600)
        uint256 successCreateBalanceGas,  // expect warm (~100)
        uint256 coldRefBalanceGas         // cold baseline (~2600)
    );

    function measureCreateCold(address coldRef) external {
        // initcode = STOP: deploys empty code and succeeds when actually run.
        bytes memory initcode = hex"00";

        // Address this contract's first CREATE (nonce == 1) would produce.
        // RLP([address(this), 1]) = 0xd6 0x94 ++ address ++ 0x01.
        address predicted = address(uint160(uint256(keccak256(
            abi.encodePacked(bytes1(0xd6), bytes1(0x94), address(this), bytes1(0x01))
        ))));

        // 1) Aborted CREATE: endowment exceeds balance, so it aborts before the
        //    address is warmed. The nonce is NOT consumed, so the next CREATE
        //    still targets `predicted`.
        uint256 tooMuch = address(this).balance + 1;
        address r1;
        assembly {
            r1 := create(tooMuch, add(initcode, 32), mload(initcode))
        }
        require(r1 == address(0), "aborted create should return 0");

        uint256 a0 = gasleft();
        uint256 _b0 = predicted.balance;   // expect COLD
        uint256 a1 = gasleft();
        _b0;

        // 2) Successful CREATE (value 0): deploys at `predicted` and warms it.
        address r2;
        assembly {
            r2 := create(0, add(initcode, 32), mload(initcode))
        }
        require(r2 == predicted, "create addr mismatch");

        uint256 b0 = gasleft();
        uint256 _b1 = predicted.balance;   // expect WARM
        uint256 b1 = gasleft();
        _b1;

        // 3) Cold baseline.
        uint256 c0 = gasleft();
        uint256 _b2 = coldRef.balance;     // expect COLD
        uint256 c1 = gasleft();
        _b2;

        emit CreateColdGasMeasured(predicted, a0 - a1, b0 - b1, c0 - c1);
    }
}
