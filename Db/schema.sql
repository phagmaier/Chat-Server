CREATE TABLE IF NOT EXISTS rooms (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL
);

-- Insert default rooms
INSERT INTO rooms (name) VALUES ('General');
INSERT INTO rooms (name) VALUES ('Tech');
INSERT INTO rooms (name) VALUES ('Biz');
INSERT INTO rooms (name) VALUES ('Culture');
INSERT INTO rooms (name) VALUES ('Fashion');

CREATE TABLE IF NOT EXISTS users (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL
);

INSERT INTO users (username, password) VALUES ('Parker', 'Hagmaier');
INSERT INTO users (username, password) VALUES ('Amber', '1234');

CREATE TABLE IF NOT EXISTS messages (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id  INTEGER,
    user_id  INTEGER,
    text     TEXT NOT NULL,
    ts       DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (room_id) REFERENCES rooms(id),
    FOREIGN KEY (user_id) REFERENCES users(id)
);

