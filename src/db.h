#pragma once
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <vector>
struct Db {
  Db(const std::string &path);
  ~Db();

  bool insertMessage(const std::string &room, const std::string &user,
                     const std::string &text);
  bool insertMessage(const std::string &room, const std::string &user,
                     const char *text);

  std::string loadMessages(const std::string &room);

  int getRoomId(const std::string &name);
  int createRoom(const std::string &name);

  int getUserId(const std::string &username);
  int createUser(const char *username, const char *password);

  bool verifyLogin(const char *username, const char *password);

  std::vector<std::string> get_rooms();
  std::vector<std::string> get_logs(int lim, int roomId);
  bool isUnique(const char *username) const;

  sqlite3 *db = nullptr;
};
