# Install hardhat and run tests

Install nvm first

```shell
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.38.0/install.sh | bash
source ~/.bashrc
```
If you use Bash Shell

```shell
source ~/.bashrc
```
If you use fish shell

```shell
curl -sL https://git.io/fisher | source && fisher install jorgebucaran/fisher                                                                                                                           
fisher install jorgebucaran/nvm.fish   
```


Now you can use NVM to run tests

```shell
nvm install 19
nvm use 19
npm install --save-dev typescript ts-node @types/node @types/mocha
npx tsc --init
npm install hardhat
npx hardhat run scripts/write_and_selfdestruct.js
npx hardhat run scripts/trace.js
```

# Build tracely

```shell
go build
```