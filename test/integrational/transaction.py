import argparse
import os
from web3_utils import Web3Utils

parser = argparse.ArgumentParser(description='Create account and run transaction')
parser.add_argument('--ipc_path', type=str,
                    help='path to ipc file', required=True)

args = parser.parse_args()
web3 = Web3Utils.get_web3(args.ipc_path)


Web3Utils.send_test_raw_transaction(web3)

exit(0)


if os.path.isfile(Web3Utils.TEST_ACCOUNT_FILE):
    account = Web3Utils.read_account_file()
else:
    account = Web3Utils.create_account(web3)
    Web3Utils.save_account(account)

Web3Utils.unlock_account(web3, account)
Web3Utils.start_mining(web3, account)

receipt = Web3Utils.send_test_transaction(web3, account)

print(receipt)
