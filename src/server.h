#pragma once

#include "db.h" // Assuming you have a db.h for your Db class
#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <istream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using boost::asio::ip::tcp;
namespace asio = boost::asio;

class ChatRoom;
class Session;

using RoomMap = std::unordered_map<std::string, std::shared_ptr<ChatRoom>>;

class Participant {
public:
  virtual ~Participant() {}
  virtual void deliver(const std::string msg) = 0;
};

using ParticipantPtr = std::shared_ptr<Participant>;

class ChatRoom {
public:
  ChatRoom(const std::string &name) : name_(name) {}

  void join(ParticipantPtr participant);

  void leave(ParticipantPtr participant);

  void broadcast(const std::string &msg, ParticipantPtr sender);
  std::string get_name() { return name_; }

private:
  std::string name_;
  int room_id;
  std::unordered_set<ParticipantPtr> participants_;
  std::mutex mutex_;
};

class Session : public Participant,
                public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket socket, RoomMap &rooms);
  ~Session();

  void start();
  void deliver(const std::string msg) override;

private:
  void read_header();
  void parse_header();
  void parse_login(std::istream &is);
  void parse_register(std::istream &is);
  void parse_message(std::istream &is);
  void parse_menu(std::istream &is);
  void parse_logs(std::istream &is);
  void parse_rooms();
  void do_write();

  tcp::socket socket_;
  asio::strand<asio::any_io_executor> strand_;
  asio::streambuf buffer_;

  std::deque<std::string> out_queue_;
  RoomMap &rooms_;
  std::shared_ptr<ChatRoom> current_room_;

  Db db_;
  std::string username_;
};

class Server {
public:
  Server(asio::io_context &io_context, uint16_t port);

private:
  void load_rooms();
  void do_accept();

  tcp::acceptor acceptor_;
  RoomMap rooms_;
};
