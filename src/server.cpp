#include "server.h"
#include <string>

void ChatRoom::join(ParticipantPtr participant) {
  std::lock_guard<std::mutex> lock(mutex_);
  participants_.insert(participant);
}

void ChatRoom::leave(ParticipantPtr participant) {
  std::lock_guard<std::mutex> lock(mutex_);
  participants_.erase(participant);
}

void ChatRoom::broadcast(const std::string &msg, ParticipantPtr sender) {
  std::vector<ParticipantPtr> recipients;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    recipients.assign(participants_.begin(), participants_.end());
  }

  for (auto &participant : recipients) {
    if (participant != sender) {
      participant->deliver(msg);
    }
  }
}

Session::Session(tcp::socket socket, RoomMap &rooms)
    : socket_(std::move(socket)), strand_(socket_.get_executor()),
      rooms_(rooms), db_(Db("../Db/chat.db")) {
  std::cout << "Session created.\n";
}

Session::~Session() {
  if (current_room_) {
    current_room_->leave(shared_from_this());
  }
  std::cout << "Session for user '" << username_ << "' destroyed.\n";
}

void Session::start() { read_header(); }

void Session::deliver(const std::string msg) {
  asio::post(strand_, [self = shared_from_this(), msg] {
    bool write_in_progress = !self->out_queue_.empty();
    self->out_queue_.push_back(msg);
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
            }
          }));
}

void Session::parse_login(std::istream &is) {
  std::string username;
  std::string password;
  std::string id;
  if (!std::getline(is, id, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, username, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, password, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  if (db_.verifyLogin(username.c_str(), password.c_str())) {
    this->username_ = username;
    id += "\nTRUE\r\n";
    deliver(id);
    return;
  }
  id += "\nFALSE\r\n";
  deliver(id);
}
void Session::parse_register(std::istream &is) {
  std::string username;
  std::string password;
  std::string id;
  if (!std::getline(is, id, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, username, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, password, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  if (password.size() && db_.isUnique(username.c_str())) {
    this->username_ = username;
    id += "\nTRUE\r\n";
    deliver(id);
    return;
  }
  id += "\nFALSE\r\n";
  deliver(id);
}

void Session::parse_message(std::istream &is) {
  if (username_.empty()) {
    deliver("ERROR\r\n");
  }
  std::string msg;
  std::string id;
  if (!std::getline(is, id, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, msg, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  std::string full_msg = "[" + username_ + "]: " + msg + "\r\n";
  if (db_.insertMessage(current_room_->get_name(), username_, full_msg)) {
    current_room_->broadcast(full_msg, shared_from_this());
    id += "\nTRUE\r\n";
    deliver(id);
    return;
  }
  deliver("ERROR\r\n");
}

void Session::parse_menu(std::istream &is) {
  std::string board;
  std::string id;
  if (!std::getline(is, id, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, board, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  auto it = rooms_.find(board);
  if (it != rooms_.end()) {
    current_room_ = it->second;
    current_room_->join(shared_from_this());
    std::string room_id = std::to_string(db_.getRoomId(board));

    id += "\n";
    id += room_id + "\r\n";
    deliver(id);
    return;
  }
  id += "\nFALSE\r\n";
  deliver(id);
}

// client must send the limit as a string
void Session::parse_logs(std::istream &is) {
  std::string lim;
  std::string id;
  if (!std::getline(is, id, '\n')) {
    deliver("ERROR\r\n");
    return;
  }
  if (!std::getline(is, lim, '\r')) {
    deliver("ERROR\r\n");
    return;
  }
  std::string room_name = current_room_->get_name();
  std::string results = db_.get_logs(std::stoi(lim), room_name);
  // will always have at least \r\n
  id += "\n" + results + "\r\n";
  deliver(id);
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
  } else if (command == "MSG") {
    parse_message(is);
  } else if (command == "REGISTER") {
    parse_register(is);
  } else if (command == "LOGS") {
    parse_logs(is);
  } else {
    std::cerr << "UNKOWN COMMAND: " << command << "\n";
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
  std::string room_names = db.get_rooms();
  std::string tmp = "";
  for (int i = 0; i < room_names.size(); ++i) {
    if (room_names[i] == '\n') {
      std::cout << "Loaded room: " << tmp << std::endl;
      rooms_[tmp] = std::make_shared<ChatRoom>(tmp);
      tmp.clear();
    } else {
      tmp += room_names[i];
    }
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
        do_accept();
      });
}
