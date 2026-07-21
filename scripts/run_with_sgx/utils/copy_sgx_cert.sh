sgx_dir=$1

mkdir -p ./tmp

cp $sgx_dir/sgx_data/cert_data/SGXServerCert.crt /skale_node_data/sgx_certs/sgx.crt
cp $sgx_dir/sgx_data/cert_data/SGXServerCert.key /skale_node_data/sgx_certs/sgx.key

cp $sgx_dir/sgx_data/cert_data/SGXServerCert.crt ./tmp/sgx.crt
cp $sgx_dir/sgx_data/cert_data/SGXServerCert.key ./tmp/sgx.key