#include "server.h"

//----------------------------------------------------------------------
// ChatRoom Implementation
//----------------------------------------------------------------------

void ChatRoom::join(ParticipantPtr participant) {
  // Lock the mutex to safely modify the participants set
  std::lock_guard<std::mutex> lock(mutex_);
  participants_.insert(participant);
  // Optional: broadcast a "user has joined" message
}

void ChatRoom::leave(ParticipantPtr participant) {
  // Lock the mutex to safely modify the participants set
  std::lock_guard<std::mutex> lock(mutex_);
  participants_.erase(participant);
  // Optional: broadcast a "user has left" message
}

// This is the core broadcast function. It runs on the calling session's strand,
// but the `deliver` calls will post work to each individual recipient's strand.
void ChatRoom::broadcast(const std::string &msg, ParticipantPtr sender) {
  std::vector<ParticipantPtr> recipients;
  {
    // Lock the mutex to safely copy the list of participants
    std::lock_guard<std::mutex> lock(mutex_);
    recipients.assign(participants_.begin(), participants_.end());
  }

  // Deliver the message to each participant.
  for (auto &participant : recipients) {
    // You might want to avoid sending the message back to the original sender
    if (participant != sender) {
      participant->deliver(msg);
    }
  }
}

//----------------------------------------------------------------------
// Session Implementation
//----------------------------------------------------------------------

Session::Session(tcp::socket socket, RoomMap &rooms)
    : socket_(std::move(socket)), strand_(socket_.get_executor()),
      rooms_(rooms),
      db_(Db("../Db/chat.db")) // Ensure your Db class is safe to be
                               // instantiated multiple times or is a singleton
{
  std::cout << "Session created.\n";
}

Session::~Session() {
  // When the session is destroyed, leave the room.
  if (current_room_) {
    current_room_->leave(shared_from_this());
  }
  std::cout << "Session for user '" << username_ << "' destroyed.\n";
}

void Session::start() { read_header(); }

// This method is called by the ChatRoom to deliver a message to this specific
// client.
void Session::deliver(const std::string &msg) {
  // Post the work to the session's strand to ensure thread safety.
  asio::post(strand_, [self = shared_from_this(), msg] {
    bool write_in_progress = !self->out_queue_.empty();
    self->out_queue_.push_back(msg + "\n"); // Add newline for client parsing
    if (!write_in_progress) {
      self->do_write();
    }
  });
}

void Session::read_header() {
  auto self = shared_from_this();
  asio::async_read_until(
      socket_, buffer_, "\r\n",
      asio::bind_executor(
          strand_, [self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              self->parse_header();
            } else {
              std::cerr << "read_header error: " << ec.message() << std::endl;
              // Session will be destroyed, automatically leaving the room.
            }
          }));
}

void Session::parse_login(std::istream &is) {
  std::string username;
  std::string password;
  if (!std::getline(is, username, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, password, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (db_.verifyLogin(username.c_str(), password.c_str())) {
    this->username_ = username;
    deliver("TRUE\r\n");
    return;
  }
  deliver("FALSE\r\n");
}
void Session::parse_register(std::istream &is) {
  std::string username;
  std::string password;
  if (!std::getline(is, username, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, password, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (password.size() && db_.isUnique(username.c_str())) {
    this->username_ = username;
    deliver("TRUE\r\n");
    return;
  }
  deliver("FALSE\r\n");
}
void Session::parse_message(std::istream &is) {
  if (!username_.empty()) {
    std::string msg;
    if (!std::getline(is, msg, '\r')) {
      deliver("ERROR\r\n");
      return;
    }
    std::string full_msg = "[" + username_ + "]: " + msg + "\n";
    current_room_->broadcast(full_msg, shared_from_this());
    return;
  }
  deliver("ERROR\r\n");
}

void Session::parse_menu(std::istream &is) {
  std::string board;
  if (!std::getline(is, board, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  auto it = rooms_.find(board);
  if (it != rooms_.end()) {
    current_room_ = it->second;
    current_room_->join(shared_from_this());
    deliver("TRUE\r\n");
    return;
  }
  deliver("FALSE\r\n");
}

void Session::parse_header() {
  std::istream is(&buffer_);

  std::string command;

  if (!std::getline(is, command, '\n')) {
    deliver("ERROR\r\n");
    return;
  }

  std::cout << "Received command: " << command << "\n";

  if (command == "LOGIN") {
    parse_login(is);
  } else if (command == "MENU") {
    parse_menu(is);
  }

  else if (command == "MSG") {
    parse_message(is);
  } else if (command == "REGISTER") {
    parse_register(is);
  } else {
    deliver("ERROR unknown_command\n");
  }
  read_header();
}

void Session::do_write() {
  auto self = shared_from_this();
  asio::async_write(
      socket_, asio::buffer(out_queue_.front()),
      asio::bind_executor(strand_, [self](boost::system::error_code ec,
                                          std::size_t /*length*/) {
        if (!ec) {
          self->out_queue_.pop_front();
          if (!self->out_queue_.empty()) {
            self->do_write(); // More messages to write
          }
        } else {
          std::cerr << "write error: " << ec.message() << std::endl;
          // Session will be destroyed, automatically leaving the room.
        }
      }));
}

//----------------------------------------------------------------------
// Server Implementation
//----------------------------------------------------------------------

Server::Server(asio::io_context &io_context, uint16_t port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
  load_rooms();
  do_accept();
  std::cout << "Server started on port " << port << std::endl;
}

void Server::load_rooms() {
  Db db("../Db/chat.db");
  std::vector<std::string> room_names = db.get_rooms();
  for (const std::string &name : room_names) {
    rooms_[name] = std::make_shared<ChatRoom>(name);
    std::cout << "Loaded room: " << name << std::endl;
  }
}

void Server::do_accept() {
  acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
          std::make_shared<Session>(std::move(socket), rooms_)->start();
        } else {
          std::cerr << "Accept error: " << ec.message() << std::endl;
        }
        // Always continue accepting new connections
        do_accept();
      });
}
