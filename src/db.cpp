#include "db.h"

Db::Db(const std::string &path) {
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Can't open Db: " << sqlite3_errmsg(db) << "\n";
    db = nullptr;
  }
}

Db::~Db() {
  if (db)
    sqlite3_close(db);
}

std::vector<std::string> Db::get_rooms() {
  sqlite3_stmt *stmt;
  std::vector<std::string> rooms;

  // Prepare the SQL statement
  if (sqlite3_prepare_v2(db, "SELECT name FROM rooms", -1, &stmt, nullptr) !=
      SQLITE_OK) {
    throw std::runtime_error("Failed to prepare statement: " +
                             std::string(sqlite3_errmsg(db)));
  }

  // Execute the statement and retrieve results
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *room =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (room) {                 // Check if room is not null
      rooms.emplace_back(room); // Use emplace_back for efficiency
    }
  }

  // Finalize the statement to release resources
  sqlite3_finalize(stmt);
  return rooms;
}

bool Db::insertMessage(const std::string &room, const std::string &user,
                       const std::string &text) {
  int roomId = getRoomId(room);
  int userId = getUserId(user);

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(
      db, "INSERT INTO messages (room_id, user_id, text) VALUES (?, ?, ?);", -1,
      &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, roomId);
  sqlite3_bind_int(stmt, 2, userId);
  sqlite3_bind_text(stmt, 3, text.c_str(), -1, SQLITE_TRANSIENT);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Db::insertMessage(const std::string &room, const std::string &user,
                       const char *text) {
  int roomId = getRoomId(room);
  int userId = getUserId(user);

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(
      db, "INSERT INTO messages (room_id, user_id, text) VALUES (?, ?, ?);", -1,
      &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, roomId);
  sqlite3_bind_int(stmt, 2, userId);
  sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::string Db::loadMessages(const std::string &room) {
  std::ostringstream output;
  sqlite3_stmt *stmt;

  int roomId = getRoomId(room);

  sqlite3_prepare_v2(db,
                     "SELECT users.username, messages.text, messages.ts "
                     "FROM messages JOIN users ON users.id = messages.user_id "
                     "WHERE messages.room_id = ? ORDER BY messages.ts ASC;",
                     -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, roomId);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *username = (const char *)sqlite3_column_text(stmt, 0);
    const char *text = (const char *)sqlite3_column_text(stmt, 1);
    const char *ts = (const char *)sqlite3_column_text(stmt, 2);
    output << "[" << ts << "] " << username << ": " << text << "\n";
  }

  sqlite3_finalize(stmt);
  return output.str();
}

int Db::getRoomId(const std::string &name) {
  sqlite3_stmt *stmt;
  int id = -1;

  sqlite3_prepare_v2(db, "SELECT id FROM rooms WHERE name = ?;", -1, &stmt,
                     nullptr);
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return id;
}

int Db::createRoom(const std::string &name) {
  sqlite3_stmt *stmt;

  sqlite3_prepare_v2(db, "INSERT INTO rooms(name) VALUES(?);", -1, &stmt,
                     nullptr);
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::cerr << "Failed to insert room: " << sqlite3_errmsg(db) << "\n";
  }

  sqlite3_finalize(stmt);
  return getRoomId(name); // return ID if needed
}

int Db::getUserId(const std::string &username) {
  sqlite3_stmt *stmt;
  int id = -1;

  sqlite3_prepare_v2(db, "SELECT id FROM users WHERE username = ?;", -1, &stmt,
                     nullptr);
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return id;
}

int Db::createUser(const char *username, const char *password) {
  sqlite3_stmt *stmt;

  sqlite3_prepare_v2(db, "INSERT INTO users(username, password) VALUES(?, ?);",
                     -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::cerr << "Failed to insert user: " << sqlite3_errmsg(db) << "\n";
  }

  sqlite3_finalize(stmt);
  return getUserId(username); // return ID if needed
}

bool Db::isUnique(const char *username) const {
  static constexpr const char *SQL =
      "SELECT 1 FROM users WHERE username = ? LIMIT 1;";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, SQL, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "isUnique-prepare: " << sqlite3_errmsg(db) << '\n';
    return false; // on any error: “not unique”
  }

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  bool ok = (rc == SQLITE_DONE); // no row → unique
  // SQLITE_ROW means a matching username exists
  // anything else is an error ⇒ treat as duplicate to be safe

  sqlite3_finalize(stmt);
  return ok;
}

std::vector<std::string> Db::get_logs(int lim, int roomId) {
  std::vector<std::string> chats;
  chats.reserve(lim);

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db,
                     "SELECT * FROM ( "
                     "  SELECT users.username, messages.text, messages.ts "
                     "  FROM messages "
                     "  JOIN users ON users.id = messages.user_id "
                     "  WHERE messages.room_id = ? "
                     "  ORDER BY messages.ts DESC "
                     "  LIMIT ? "
                     ") sub "
                     "ORDER BY ts ASC;",
                     -1, &stmt, nullptr);

  // Bind room ID
  sqlite3_bind_int(stmt, 1, roomId);

  // Bind message count
  sqlite3_bind_int(stmt, 2, lim);

  std::string txt;
  std::string name;
  std::string full;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    name = (const char *)sqlite3_column_text(stmt, 0);
    txt = (const char *)sqlite3_column_text(stmt, 1);
    // const char *ts = (const char *)sqlite3_column_text(stmt, 2);
    full = "[" + name + "]: " + txt;
    chats.push_back(full);
  }
  sqlite3_finalize(stmt);
  return chats;
}

bool Db::verifyLogin(const char *username, const char *password) {
  sqlite3_stmt *stmt;
  bool valid = false;

  sqlite3_prepare_v2(db,
                     "SELECT 1 FROM users WHERE username = ? AND password = ?;",
                     -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    valid = true;
  }

  sqlite3_finalize(stmt);
  return valid;
}
