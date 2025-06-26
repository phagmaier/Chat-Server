#include "client.h"

Client::Client(asio::io_context &ctx)
    : socket_(ctx), strand_(socket_.get_executor()) {}

void Client::connect(const std::string &host, const std::string &port) {
  tcp::resolver r(socket_.get_executor());
  auto endpoints = r.resolve(host, port);
  asio::async_connect(
      socket_, endpoints,
      [self = shared_from_this()](boost::system::error_code ec,
                                  const tcp::endpoint &) {
        if (ec) {
          std::cerr << "Connect failed: " << ec.message() << '\n';
          return;
        }
        std::cout << "Connected. Type /room <name> to switch rooms.\n";
        self->read();
      });
}

void Client::shutdown() {
  boost::system::error_code dummy;
  socket_.close(dummy);
  std::cerr << "Disconnected." << std::endl;
}

void Client::read_msgg() {
  auto self = shared_from_this();
  asio::async_read_until(
      socket_, buffer_, "\r\n",
      asio::bind_executor(
          strand_, [self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              self->parse_header();
            } else {
              std::cerr << "read_header error: " << ec.message() << std::endl;
            }
          }));
}
