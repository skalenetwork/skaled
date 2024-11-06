# Run skaled for tests 

```
build/skaled/skaled --config test/historicstate/configs/basic_config.json
```

# Run local geth for tests 

First install web3

```
pip3 install web3
```

Then run geth container 

```
cd test/historicstate/hardhat
python3 run_geth.py
```


# Install hardhat and run tests

```shell
sudo apt install nodejs
npm install
```

Now run test against skaled

```shell
npx hardhat run scripts/trace.ts --network skaled
```

To run the same test against geth

```shell
npx hardhat run scripts/trace.ts --network geth
```

