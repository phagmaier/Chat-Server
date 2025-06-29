#include "server.h"
constexpr int PORT__ = 1234;
int main() {
  try {
    asio::io_context ctx(1);
    Server srv(ctx, PORT__);
    std::cout << "Chat server listening on : " << PORT__ << "\n";
    ctx.run();
  } catch (std::exception &ex) {
    std::cerr << "Fatal: " << ex.what() << '\n';
  }
}
