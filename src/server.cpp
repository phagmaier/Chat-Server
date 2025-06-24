#include "server.h"
/********************SERVER********************/
Server::Server(asio::io_context &ctx, uint16_t port)
    : acceptor_(ctx, tcp::endpoint(tcp::v4(), port)) {
  get_rooms();
  accept();
}

void Server::get_rooms() {
  Db db("../Db/chat.db");
  std::vector<std::string> tmp = db.get_rooms();
  for (std::string &str : tmp) {
    rooms[str] = {};
  }
}

void Server::accept() {
  acceptor_.async_accept([this](boost::system::error_code ec,
                                tcp::socket sock) {
    if (!ec) {
      std::make_shared<Session>(std::move(sock), rooms)->start();
      accept();
    } else {
      std::cerr << "ERR: IDK HOW TO HANDLE BUT COULD NOT ACCEPT CONNECTION\n";
    }
  });
}
/********************SERVER********************/

/********************Session********************/

Session::Session(tcp::socket sock, ROOMS &rooms)
    : socket_(std::move(sock)), strand_(socket_.get_executor()), rooms{rooms},
      db{Db("../Db/chat.db")} {
  start();
}

void Session::start() { read_header(); }

void Session::read_header() {
  auto self = shared_from_this();
  asio::async_read_until(
      socket_, header_buf_, '\0',
      asio::bind_executor(strand_,
                          [self](boost::system::error_code ec, std::size_t) {
                            if (ec) {
                              std::cerr << "HANDLE ERRORS LATER\n";
                              return;
                            } else {
                              self->parse_header();
                            }
                          }));
}

// can change this so you just send 1 byte a char that indicates the header
// but leave for now to make it easier
void Session::parse_header() {
  std::istream is(&header_buf_);
  std::string header_content;
  std::getline(is, header_content, '\0');
  if (header_content == "login") {
    read_login();
  } else if (header_content == "register") {
    read_register();
  } else if (header_content == "menu") {
    read_menu();
  } else if (header_content == "leave") {
    std::cerr << "leaving chat room. idk how to hand yet\n";
  } else if (header_content == "DM") {
    std::cerr << "haven't implimented dm yet\n";
  } else {
    std::cerr << "error no matching header\n";
  }
}

void Session::parse_login() {
  std::istream is(&header_buf_);
  std::string usr;
  std::string pass;
  std::getline(is, usr, '\n');
  std::getline(is, pass, '\0');
  if (db.verifyLogin(usr.c_str(), pass.c_str())) {
    out_queue_.push_back("true");
  } else {
    out_queue_.push_back("false");
  }
}

void Session::parse_register() {
  std::istream is(&header_buf_);
  std::string usr;
  std::string pass;
  std::getline(is, usr, '\n');
  std::getline(is, pass, '\0');
  if (db.isUnique(usr.c_str())) {
    db.createUser(usr.c_str(), pass.c_str());
    out_queue_.push_back("true");
  } else {
    out_queue_.push_back("false");
  }
}

void Session::read_login() {
  auto self = shared_from_this();
  asio::async_read_until(
      socket_, header_buf_, '\0',
      asio::bind_executor(strand_,
                          [self](boost::system::error_code ec, std::size_t) {
                            if (ec) {
                              std::cerr << "HANDLE ERRORS LATER\n";
                              std::cerr << ec.to_string() << "\n";
                              return;
                            } else {
                              self->parse_login();
                              self->write_response();
                            }
                          }));
}

void Session::read_register() {
  auto self = shared_from_this();
  asio::async_read_until(
      socket_, header_buf_, '\0',
      asio::bind_executor(strand_,
                          [self](boost::system::error_code ec, std::size_t) {
                            if (ec) {
                              std::cerr << "HANDLE ERRORS LATER\n";
                              std::cerr << ec.to_string() << "\n";
                              return;
                            } else {
                              self->parse_register();
                              self->write_response();
                            }
                          }));
}

void Session::write_response() {
  auto self = shared_from_this();
  asio::async_write(
      socket_, asio::buffer(out_queue_.front()),
      asio::bind_executor(strand_,
                          [self](boost::system::error_code ec, std::size_t) {
                            if (ec) {
                              // self->room_.leave(self);
                              std::cerr << "ERROR WRITING\n";
                              std::cerr << ec.to_string() << "\n";
                              return;
                            }
                            self->out_queue_.pop_front();
                            if (!self->out_queue_.empty()) {
                              self->write_response();
                            } else {
                              self->read_header();
                            }
                          }));
}

/********************Session********************/
