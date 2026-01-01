#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

extern sqlite3* g_db;

struct User {
    int id = -1;
    std::string username;
};

struct Post {
    int id = -1;
    std::string title;
    std::string content;
    std::string secret;     
    std::string image_path;
    int author_id = -1;
};

bool db_init(const char* db_path = "./data/news.db");
bool db_create_user(const std::string& username, const std::string& password);
User db_get_user_by_credentials(const std::string& username, const std::string& password);
User db_get_user_by_id(int id);
bool db_create_post(const std::string& title, const std::string& content, const std::string& secret, const std::string& image_path, int author_id);
std::vector<Post> db_get_all_posts();
Post db_get_post_by_id(int id);
void db_close();