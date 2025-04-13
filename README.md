You will have to install Quiche first:<br />
git clone --recursive https://github.com/cloudflare/quiche <br />
cd quiche <br />
cargo build --release --features pkg-config-meta <br />
then create certificates for the server: <br />
openssl req -x509 -newkey rsa:4096 -keyout cert.key -out cert.crt -days 365 -nodes <br />
then combile: <br />
g++ -o quic_server server.cpp -lquiche -lpthread -std=c++17 <br />
g++ -o quic_client client.cpp -lquiche -lpthread -std=c++17 <br />
