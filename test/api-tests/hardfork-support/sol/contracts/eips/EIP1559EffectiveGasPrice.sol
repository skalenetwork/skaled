// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract EIP1559EffectiveGasPrice {
    event GasPriceReported(uint256 gasPrice);

    function reportGasPrice() external returns (uint256) {
        uint256 price = tx.gasprice;
        emit GasPriceReported(price);
        return price;
    }
}
