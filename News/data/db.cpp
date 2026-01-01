#include "db.h"
#include <iostream>
#include <sstream>

sqlite3* g_db = nullptr;

bool db_init(const char* db_path) {
    if (sqlite3_open(db_path, &g_db) != SQLITE_OK) {
        std::cerr << "❌ Не удалось открыть БД\n";
        return false;
    }

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS posts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            content TEXT NOT NULL,
            secret TEXT NOT NULL,
            image_path TEXT NOT NULL,
            author_id INTEGER NOT NULL,
            FOREIGN KEY(author_id) REFERENCES users(id)
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(g_db, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "❌ Ошибка создания таблиц: " << err << "\n";
        sqlite3_free(err);
        sqlite3_close(g_db);
        return false;
    }

    sqlite3_stmt* stmt;
    int count = 0;
    sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM users", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool db_create_user(const std::string& username, const std::string& password) {
    const char* sql = "INSERT INTO users (username, password) VALUES (?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

User db_get_user_by_credentials(const std::string& username, const std::string& password) {
    const char* sql = "SELECT id, username FROM users WHERE username = ? AND password = ?";
    sqlite3_stmt* stmt;
    User u;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return u;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u.id = sqlite3_column_int(stmt, 0);
        u.username = (const char*)sqlite3_column_text(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return u;
}

User db_get_user_by_id(int id) {
    const char* sql = "SELECT username FROM users WHERE id = ?";
    sqlite3_stmt* stmt;
    User u;
    u.id = id;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return u;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        u.username = (const char*)sqlite3_column_text(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return u;
}

bool db_create_post(const std::string& title, const std::string& content, const std::string& secret, const std::string& image_path, int author_id) {
    const char* sql = "INSERT INTO posts (title, content, secret, image_path, author_id) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, secret.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, image_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, author_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Post> db_get_all_posts() {
    std::vector<Post> posts;
    const char* sql = "SELECT id, title, content, secret, image_path, author_id FROM posts ORDER BY id DESC";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return posts;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Post p;
        p.id = sqlite3_column_int(stmt, 0);
        p.title = (const char*)sqlite3_column_text(stmt, 1);
        p.content = (const char*)sqlite3_column_text(stmt, 2);
        p.secret = (const char*)sqlite3_column_text(stmt, 3);
        p.image_path = (const char*)sqlite3_column_text(stmt, 4);
        p.author_id = sqlite3_column_int(stmt, 5);
        posts.push_back(p);
    }
    sqlite3_finalize(stmt);
    return posts;
}

Post db_get_post_by_id(int id) {
    Post p;
    const char* sql = "SELECT title, content, secret, image_path, author_id FROM posts WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return p;
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        p.id = id;
        p.title = (const char*)sqlite3_column_text(stmt, 0);
        p.content = (const char*)sqlite3_column_text(stmt, 1);
        p.secret = (const char*)sqlite3_column_text(stmt, 2);
        p.image_path = (const char*)sqlite3_column_text(stmt, 3);
        p.author_id = sqlite3_column_int(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return p;
}

void db_close() {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}