#pragma once

#include "db.h" // Assuming you have a db.h for your Db class
#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using boost::asio::ip::tcp;
namespace asio = boost::asio;

// Forward declarations
class ChatRoom;
class Session;

// A type definition for a map of room names to ChatRoom objects.
// Using shared_ptr to manage the lifecycle of rooms.
using RoomMap = std::unordered_map<std::string, std::shared_ptr<ChatRoom>>;

//----------------------------------------------------------------------
// Participant Interface
// Represents a participant in a chat room. This allows the ChatRoom
// to be decoupled from the Session implementation.
//----------------------------------------------------------------------

class Participant {
public:
  virtual ~Participant() {}
  virtual void deliver(const std::string &msg) = 0;
};

using ParticipantPtr = std::shared_ptr<Participant>;

//----------------------------------------------------------------------
// Chat Room
// Manages a collection of participants and delivers messages to them.
//----------------------------------------------------------------------

class ChatRoom {
public:
  ChatRoom(const std::string &name) : name_(name) {}

  // Adds a participant to the room. Thread-safe.
  void join(ParticipantPtr participant);

  // Removes a participant from the room. Thread-safe.
  void leave(ParticipantPtr participant);

  // Delivers a message to all participants in the room. Thread-safe.
  void broadcast(const std::string &msg, ParticipantPtr sender);

private:
  std::string name_;
  std::unordered_set<ParticipantPtr> participants_;
  std::mutex mutex_; // Mutex to protect access to the participants set
};

//----------------------------------------------------------------------
// Session
// Represents a single client connection.
//----------------------------------------------------------------------

class Session : public Participant,
                public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket socket, RoomMap &rooms);
  ~Session();

  void start();
  void deliver(const std::string &msg) override;

private:
  void read_header();
  void parse_header();
  void parse_login(std::istream &is);
  void parse_register(std::istream &is);
  void parse_message(std::istream &is);
  void parse_menu(std::istream &is);
  void do_write();

  // Member variables
  tcp::socket socket_;
  // UPDATED: Use a type-erased executor for the strand to resolve compiler
  // errors.
  asio::strand<asio::any_io_executor> strand_;
  asio::streambuf buffer_;

  std::deque<std::string> out_queue_;
  RoomMap &rooms_; // Reference to the server's map of all rooms
  std::shared_ptr<ChatRoom>
      current_room_; // The room this session is currently in

  Db db_;
  std::string username_;
};

//----------------------------------------------------------------------
// Server
// Accepts incoming client connections.
//----------------------------------------------------------------------

class Server {
public:
  Server(asio::io_context &io_context, uint16_t port);

private:
  void load_rooms();
  void do_accept();

  tcp::acceptor acceptor_;
  RoomMap rooms_; // Owns all the chat rooms
};
