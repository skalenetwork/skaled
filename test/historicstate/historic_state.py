import web3.eth
from web3 import Web3, HTTPProvider
import json
import unittest
import codecs

insecure_test_key = codecs.decode(b'bd200f4e7f597f3c2c77fb405ee7fabeb249f63f03f43d5927b4fa0c43cfe85e')

class MyTestCase(unittest.TestCase):
    def test_transaction(self):
        w3 = Web3(Web3.HTTPProvider("http://localhost:1234"))
        nonce = w3.eth.get_transaction_count('0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6')
        print("Nonce:" + str(nonce))

        
        #sign the transaction
        signed_tx =  w3.eth.account.sign_transaction(dict(
            nonce=w3.eth.get_transaction_count('0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6'),
            gas=100000,
            gasPrice = w3.eth.gas_price,
            to='0x907cd0881E50d359bb9Fd120B1A5A143b1C97De6',
            value= 1,
            data=b'',
            chainId=0x12345,
        ), insecure_test_key)

        tx_hash = w3.eth.send_raw_transaction(signed_tx.rawTransaction)

if __name__ == '__main__':
    unittest.main()
