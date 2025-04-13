# QUIC Server/Client Example

This repository contains a simple example of a QUIC server and client using [Cloudflare's Quiche](https://github.com/cloudflare/quiche).

## Installing Quiche

You need to install Quiche first. Run the following commands in your terminal:

```bash
git clone --recursive https://github.com/cloudflare/quiche
cd quiche
cargo build --release --features pkg-config-meta

Creating Certificates for the Server
Generate a self-signed certificate and private key (valid for 365 days) by running:

bash
Copy
openssl req -x509 -newkey rsa:4096 -keyout cert.key -out cert.crt -days 365 -nodes
csharp
Copy

Simply copy this content into your `README.md` file.









