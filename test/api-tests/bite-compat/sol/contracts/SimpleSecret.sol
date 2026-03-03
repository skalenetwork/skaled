pragma solidity >=0.8.27;

import { Address } from "@openzeppelin/contracts/utils/Address.sol";
import { BITE } from "@skalenetwork/bite-solidity/BITE.sol";
import { IBiteSupplicant } from "@skalenetwork/bite-solidity/interfaces/IBiteSupplicant.sol";

contract SimpleSecret is IBiteSupplicant {
    using Address for address payable;

    bytes public decryptedMessage;

    uint256 public constant CTX_GAS_LIMIT = 2500000;
    uint256 public constant CTX_GAS_PAYMENT = 0.06 ether;

    error AccessViolation();

    // Submit encrypted secret for decryption
    function revealSecret(bytes calldata encrypted) external payable {
        require(msg.value == CTX_GAS_PAYMENT, "Invalid CTX gas payment");

        bytes[] memory encryptedArgs = new bytes[](1);
        encryptedArgs[0] = encrypted;

        bytes[] memory plaintextArgs = new bytes[](0);

        // Submit CTX - returns address that will call onDecrypt
        address payable ctxSender = BITE.submitCTX(
            BITE.SUBMIT_CTX_ADDRESS,
            CTX_GAS_LIMIT,
            encryptedArgs,
            plaintextArgs
        );

        // Refund any unused ETH
        payable(ctxSender).sendValue(msg.value);
    }

    // Called by SKALE consensus in next block with decrypted data
    function onDecrypt(
        bytes[] calldata decryptedArgs,
        bytes[] calldata /* plaintextArgs */
    ) external override {
        decryptedMessage = decryptedArgs[0];
    }

    function getSecret() external view returns (bytes memory) {
        return decryptedMessage;
    }
    receive() external payable {}
    fallback() external payable {}
}
