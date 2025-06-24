#pragma once
#include "db.h"
#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
using ROOMS =
    std::unordered_map<std::string,
                       std::unordered_set<std::shared_ptr<tcp::socket>>>;

/********************Session********************/
class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket sock, ROOMS &rooms);
  void start();

  // for parsing they should actually
  // always send the whole message all at once
  // and you'll just seperate it by newline
  // so it'll be the header newline then you parse
  // accordingly
private:
  void read_header();
  void read_login();
  void read_register();
  void parse_header();
  void parse_login();
  void parse_register();
  void write_response();
  void read_menu();
  void read_mmsg();

  tcp::socket socket_;
  asio::strand<asio::any_io_executor> strand_;
  // may be cheaper to store pointer if
  // many sessions but worry about that later
  ROOMS &rooms;
  Db db;

  boost::asio::streambuf header_buf_;
  std::deque<std::string> out_queue_;
};
/********************Session********************/

/********************SERVER********************/
class Server {
public:
  Server(asio::io_context &ctx, uint16_t port);

private:
  void accept();
  void get_rooms();
  tcp::acceptor acceptor_;
  ROOMS rooms;
};
/********************SERVER********************/
