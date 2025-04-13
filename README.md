You will have to install Quiche first:
git clone --recursive https://github.com/cloudflare/quiche
cd quiche
cargo build --release --features pkg-config-meta
then create certificates for the server:
openssl req -x509 -newkey rsa:4096 -keyout cert.key -out cert.crt -days 365 -nodes
then combile:
g++ -o quic_server server.cpp -lquiche -lpthread -std=c++17
g++ -o quic_client client.cpp -lquiche -lpthread -std=c++17
