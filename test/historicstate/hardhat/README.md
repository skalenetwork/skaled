# Install hardhat and run tests


```shell
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.38.0/install.sh | bash
source ~/.bashrc
nvm install 19
nvm use 19
npx hardhat run scripts/write_and_selfdestruct_test.js
```

# Build tracely

```shell
go build
```