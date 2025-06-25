#include <algorithm>
#include <boost/asio.hpp>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using boost::asio::ip::tcp;
namespace asio = boost::asio;

constexpr std::size_t HEADER_LEN = 2;
constexpr std::size_t MAX_BODY = 4096;

class ChatRoom;

class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket sock, ChatRoom &room);
  void start();
  void deliver(const std::string &msg);

private:
  void do_read_header();
  void do_read_body();
  void do_write();

  tcp::socket socket_;
  asio::strand<asio::any_io_executor> strand_;
  ChatRoom &room_;

  std::array<char, HEADER_LEN> header_buf_{};
  std::vector<char> body_buf_;
  std::deque<std::string> out_queue_;
};

class ChatRoom {
public:
  explicit ChatRoom(std::initializer_list<std::string> rooms) {
    valid_.insert(rooms.begin(), rooms.end());
    for (auto &r : valid_)
      rooms_[r];
  }

  bool is_valid(const std::string &r) const { return valid_.count(r); }

  void join(const std::string &room, const std::shared_ptr<Session> &client) {
    if (!is_valid(room))
      return;
    rooms_[room].insert(client);
  }

  void leave(const std::shared_ptr<Session> &client) {
    for (auto &[_, participants] : rooms_)
      participants.erase(client);
  }

  void broadcast(const std::string &room, const std::string &msg) {
    if (!is_valid(room))
      return;
    for (auto &p : rooms_.at(room))
      p->deliver(msg);
  }

private:
  std::unordered_set<std::string> valid_;
  std::unordered_map<std::string, std::unordered_set<std::shared_ptr<Session>>>
      rooms_;
};

Session::Session(tcp::socket sock, ChatRoom &room)
    : socket_(std::move(sock)), strand_(socket_.get_executor()), room_(room) {}

void Session::start() { do_read_header(); }

void Session::deliver(const std::string &msg) {
  uint16_t len = static_cast<uint16_t>(msg.size());
  std::string packet(HEADER_LEN + len, '\0');
  packet[0] = static_cast<char>((len >> 8) & 0xFF);
  packet[1] = static_cast<char>(len & 0xFF);
  std::memcpy(packet.data() + HEADER_LEN, msg.data(), len);

  asio::post(strand_, [self = shared_from_this(), p = std::move(packet)] {
    bool busy = !self->out_queue_.empty();
    self->out_queue_.push_back(std::move(p));
    if (!busy)
      self->do_write();
  });
}

void Session::do_read_header() {
  auto self = shared_from_this();
  asio::async_read(
      socket_, asio::buffer(header_buf_),
      asio::bind_executor(
          strand_, [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
              self->room_.leave(self);
              return;
            }

            uint16_t len = (static_cast<uint8_t>(self->header_buf_[0]) << 8) |
                           static_cast<uint8_t>(self->header_buf_[1]);
            if (len == 0 || len > MAX_BODY) {
              self->socket_.close();
              return;
            }
            self->body_buf_.resize(len);
            self->do_read_body();
          }));
}

void Session::do_read_body() {
  auto self = shared_from_this();
  asio::async_read(
      socket_, asio::buffer(body_buf_),
      asio::bind_executor(
          strand_, [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
              self->room_.leave(self);
              return;
            }

            auto nl =
                std::find(self->body_buf_.begin(), self->body_buf_.end(), '\n');
            std::string room(self->body_buf_.begin(), nl);
            if (!self->room_.is_valid(room)) {
              self->socket_.close();
              return;
            }

            std::string msg;
            if (nl != self->body_buf_.end())
              msg.assign(nl + 1, self->body_buf_.end());

            self->room_.join(room, self);
            self->room_.broadcast(room, msg);
            self->do_read_header();
          }));
}

void Session::do_write() {
  auto self = shared_from_this();
  asio::async_write(
      socket_, asio::buffer(out_queue_.front()),
      asio::bind_executor(strand_,
                          [self](boost::system::error_code ec, std::size_t) {
                            if (ec) {
                              self->room_.leave(self);
                              return;
                            }
                            self->out_queue_.pop_front();
                            if (!self->out_queue_.empty())
                              self->do_write();
                          }));
}

class Server {
public:
  Server(asio::io_context &ctx, uint16_t port)
      : acceptor_(ctx, tcp::endpoint(tcp::v4(), port)),
        room_({"general", "tech", "music", "sports"}) {
    accept();
  }

private:
  void accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket sock) {
          if (!ec)
            std::make_shared<Session>(std::move(sock), room_)->start();
          accept();
        });
  }

  tcp::acceptor acceptor_;
  ChatRoom room_;
};

int main() {
  try {
    asio::io_context ctx(1);
    Server srv(ctx, 1234);
    std::cout << "Chat server listening on :1234\n";
    ctx.run();
  } catch (std::exception &ex) {
    std::cerr << "Fatal: " << ex.what() << '\n';
  }
}
