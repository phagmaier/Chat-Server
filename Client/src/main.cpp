#include "client.h"

int main(int argc, char *argv[]) {
  const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
  const char *port = (argc > 2) ? argv[2] : "1234";

  asio::io_context ctx;
  auto client = std::make_shared<ClientSession>(ctx);
  client->connect(host, port);

  // run IO on a separate thread so std::getline is non‑blocking
  std::thread io_thread([&ctx] { ctx.run(); });

  std::string current_room;
  std::string line;
  std::cout << "> " << std::flush;

  while (std::getline(std::cin, line)) {
    if (starts_with(line)) {
      current_room = line.substr(6);
      std::cout << "Switched to room '" << current_room << "'\n> "
                << std::flush;
      continue;
    }
    if (current_room.empty()) {
      std::cout << "Select a room first with /room <name>\n> " << std::flush;
      continue;
    }
    std::string payload = current_room + '\n' + line;
    client->send(payload);
    std::cout << "> " << std::flush;
  }

  ctx.stop();
  io_thread.join();
  return 0;
}
