from time import sleep
from web3 import Web3
from web3.auto import w3
import os
import binascii


class Web3Utils:
    TEST_PASSWORD = os.environ['TEST_PASSWORD']
    TEST_KEY_FILE = os.environ['TEST_KEY_FILE']
    TEST_ACCOUNT_FILE = 'TEST_ACCOUNT'

    @staticmethod
    def get_web3(ipc_path):
#        ipc = Web3.IPCProvider(ipc_path, timeout=300)
        ipc = Web3.HTTPProvider(ipc_path)
        return Web3(ipc)

    @classmethod
    def save_account(cls, acc):
        with open(cls.TEST_ACCOUNT_FILE, 'w') as f:
            f.write(acc)

    @classmethod
    def create_account(cls, web3):
        return web3.personal.newAccount(cls.TEST_PASSWORD)

    @classmethod
    def unlock_account(cls, web3, account):
        web3.personal.unlockAccount(account, cls.TEST_PASSWORD, 0)

    @classmethod
    def read_account_file(cls):
        return open(cls.TEST_ACCOUNT_FILE).read()

    @staticmethod
    def send_test_transaction(web3, unlocked_account):
        tx_hash = web3.eth.sendTransaction({
            'from': unlocked_account,
            'to': '0x5648C4355c184eB6a2CE7a75A659d89004D3b56a',
            'value': 100000000
        })
        sleep(3)
        return web3.eth.getTransactionReceipt(tx_hash)

    @staticmethod
    def send_test_raw_transaction(web3):
        with open(Web3Utils.TEST_KEY_FILE) as keyfile:
            encrypted_key = keyfile.read()

        private_key = w3.eth.account.decrypt(encrypted_key, Web3Utils.TEST_PASSWORD)
        address = w3.eth.account.privateKeyToAccount(private_key).address
        print(f'account: {address}')
        # print(private_key)
        nonce = web3.eth.getTransactionCount(address)
        # nonce = 1
        print(f'nonce: {nonce}')
        balance = web3.eth.getBalance(address)
        print('balance: %s' % (balance))
        transaction = {
            'from': address,
#            'to': '0xF0109fC8DF283027b6285cc889F5aA624EaC1F55',
            'to': '0x20Bfa72Da19D13a8C7c771dFA9A41b780C35ee7b',
            'value': 1000000000000000000,
            'gas': 2000000,
            'gasPrice': 1,
            'nonce': nonce,
            # 'chainId': 1
        }
        # key = Web3Utils.eth_hex_str(private_key)

        signed = w3.eth.account.signTransaction(transaction, private_key=private_key)
        raw_transaction = Web3Utils.eth_hex_str(signed.rawTransaction)  # binascii.hexlify(signed.rawTransaction)
        print(f'raw_transaction: {raw_transaction}')
        print()
        tx_hash = web3.eth.sendRawTransaction(raw_transaction)
        print(f'tx_hash: {tx_hash}')

    @staticmethod
    def start_mining(web3, account):
        web3.miner.setEtherBase(account)
        web3.miner.start(1)

    @staticmethod
    def eth_hex_str(hex_bytes):
        return f'0x{binascii.hexlify(hex_bytes).decode("utf-8")}'
