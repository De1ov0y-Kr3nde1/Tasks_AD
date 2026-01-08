#include "httplib.h"
#include "data/db.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <memory>

std::unordered_map<std::string, int> g_active_tokens;

std::string generate_auth_token() {
    std::string charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string token;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.size() - 1);
    for (int i = 0; i < 32; ++i)
        token += charset[dis(gen)];
    return token;
}

std::string build_top_bar(const std::string& token) {
    std::ostringstream html;
    html << "<header style=\"background:#1a1a2e; color:white; padding:12px; display:flex; justify-content:space-between; align-items:center; font-family:Arial,sans-serif;\">";
    html << "<div><strong>Actual_News</strong></div>";

    if (!token.empty() && g_active_tokens.find(token) != g_active_tokens.end()) {
        int user_id = g_active_tokens.at(token);
        User u = db_get_user_by_id(user_id);
        html << "<div>";
        html << "👤 " << u.username << " | ";
        html << "<a href='/profile' style='color:white; margin-left:12px;'>Профиль</a> | ";
        html << "<a href='/logout' style='color:white; margin-left:12px;'>Выйти</a>";
        html << "</div>";
    } else {
        html << "<div>";
        html << "<a href='/login.html' style='color:white;'>Войти</a> | ";
        html << "<a href='/register.html' style='color:white; margin-left:12px;'>Регистрация</a>";
        html << "</div>";
    }
    html << "</header>";
    return html.str();
}

std::string fetch_remote_metadata(const std::string& img_url, int post_author_id, int current_user_id) {
    if (current_user_id != post_author_id) {
        return ""; 
    }

    if (img_url.empty() || (img_url.substr(0, 7) != "http://" && img_url.substr(0, 8) != "https://")) {
        return "";
    }

    try {
        size_t delim = img_url.find('/', 8);
        if (delim == std::string::npos) return "";

        std::string host_part = img_url.substr(0, delim);
        std::string path_part = img_url.substr(delim);

        if (host_part.substr(0, 7) == "http://") {
            host_part = host_part.substr(7);
        } else if (host_part.substr(0, 8) == "https://") {
            host_part = host_part.substr(8);
        }

        httplib::Client client(host_part.c_str());
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(5, 0);

        auto response = client.Get(path_part.c_str());
        if (!response || response->status != 200 || response->body.empty()) {
            return "";
        }

        const std::string& raw_data = response->body;

        if (raw_data.size() < 64) {
            return "";
        }

        bool valid_format = false;
        if (raw_data.size() >= 4) {
            if (raw_data[0] == (char)0xFF && raw_data[1] == (char)0xD8 && raw_data[2] == (char)0xFF) {
                valid_format = true;
            } else if (raw_data.size() >= 8 &&
                       memcmp(raw_data.data(), "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {
                valid_format = true;
            }
        }

        if (!valid_format) {
            return "";
        }

        std::string output;
        FILE* stream = popen(raw_data.c_str(), "r");
        if (stream) {
            char chunk[1024];
            while (fgets(chunk, sizeof(chunk), stream) != nullptr) {
                output += chunk;
            }
            pclose(stream);
        }
        return output;

    } catch (...) {
    }
    return "";
}

bool is_image_source_valid(const std::string& url) {
    if (url.empty() || (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://")) {
        return false;
    }

    try {
        size_t delim = url.find('/', 8);
        if (delim == std::string::npos) return false;

        std::string host_part = url.substr(0, delim);
        std::string path_part = url.substr(delim);
        if (host_part.substr(0, 7) == "http://") host_part = host_part.substr(7);
        else if (host_part.substr(0, 8) == "https://") host_part = host_part.substr(8);

        httplib::Client client(host_part.c_str());
        client.set_connection_timeout(5, 0);
        client.set_read_timeout(5, 0);

        auto response = client.Get(path_part.c_str());
        if (!response || response->status != 200 || response->body.empty()) return false;

        const std::string& data = response->body;

        if (data.size() < 64) {
            return false;
        }

        if (data.size() >= 4) {
            if ((data[0] == (char)0xFF && data[1] == (char)0xD8 && data[2] == (char)0xFF) ||
                (data.size() >= 8 && memcmp(data.data(), "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0)) {
                return true;
            }
        }
    } catch (...) {}
    return false;
}

int main() {
    if (!db_init()) {
        std::cerr << "❌ Не удалось инициализировать БД\n";
        return 1;
    }

    httplib::Server web_server;

    web_server.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Cookie");
        if (token.substr(0, 8) == "session=") token = token.substr(8);

        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Actual_News</title></head><body>";
        html << build_top_bar(token);
        html << "<div style='padding:20px; font-family:Arial,sans-serif;'>";

        html << "<h2>Новости</h2>";
        auto posts = db_get_all_posts();
        for (const auto& p : posts) {
            html << "<div style=\"display:flex; gap:16px; margin-bottom:24px; align-items:flex-start; max-width:800px;\">";
            html << "<div style=\"flex-shrink:0; width:200px; height:130px; overflow:hidden; border-radius:6px;\">";
            html << "<img src=\"" << p.image_path << "\" style=\"width:100%; height:100%; object-fit:cover;\">";
            html << "</div>";
            html << "<div style=\"flex:1;\">";
            html << "<h3><a href='/view?id=" << p.id << "' style='color:#1a1a2e; text-decoration:none;'>" << p.title << "</a></h3>";
            html << "</div>";
            html << "</div>";
        }

        html << "</div></body></html>";
        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    web_server.Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        if (username.empty() || password.empty()) {
            res.set_redirect("/register.html?error=Заполните все поля");
            return;
        }
        if (db_create_user(username, password)) {
            User u = db_get_user_by_credentials(username, password);
            if (u.id != -1) {
                std::string token = generate_auth_token();
                g_active_tokens[token] = u.id;
                res.set_header("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
            }
            res.set_redirect("/");
        } else {
            res.set_redirect("/register.html?error=Пользователь уже существует");
        }
    });

    web_server.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.get_param_value("username");
        std::string password = req.get_param_value("password");
        User u = db_get_user_by_credentials(username, password);
        if (u.id != -1) {
            std::string token = generate_auth_token();
            g_active_tokens[token] = u.id;
            res.set_header("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
            res.set_redirect("/");
        } else {
            res.set_redirect("/login.html?error=Неверный логин или пароль");
        }
    });

    web_server.Get("/logout", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Cookie");
        if (token.substr(0, 8) == "session=") {
            token = token.substr(8);
            g_active_tokens.erase(token);
        }
        res.set_redirect("/");
    });

    web_server.Get("/profile", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Cookie");
        if (token.substr(0, 8) == "session=") token = token.substr(8);
        if (g_active_tokens.find(token) == g_active_tokens.end()) {
            res.set_redirect("/login.html");
            return;
        }
        int user_id = g_active_tokens[token];
        User u = db_get_user_by_id(user_id);

        std::vector<Post> user_posts;
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id, title, content, secret, image_path, author_id FROM posts WHERE author_id = ? ORDER BY id DESC";
        sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, user_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Post p;
            p.id = sqlite3_column_int(stmt, 0);
            p.title = (const char*)sqlite3_column_text(stmt, 1);
            p.content = (const char*)sqlite3_column_text(stmt, 2);
            p.secret = (const char*)sqlite3_column_text(stmt, 3);
            p.image_path = (const char*)sqlite3_column_text(stmt, 4);
            p.author_id = sqlite3_column_int(stmt, 5);
            user_posts.push_back(p);
        }
        sqlite3_finalize(stmt);

        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Профиль</title></head><body>";
        html << build_top_bar(token);
        html << "<div style='padding:20px; max-width:800px; margin:0 auto; font-family:Arial,sans-serif;'>";

        html << "<h2>Профиль: " << u.username << "</h2>";
        html << "<p>Опубликовано статей: <strong>" << user_posts.size() << "</strong></p>";

        if (user_posts.empty()) {
            html << "<p>У вас пока нет опубликованных статей.</p>";
        } else {
            html << "<h3>Ваши статьи:</h3><ul>";
            for (const auto& p : user_posts) {
                html << "<li>";
                html << "<strong>" << p.title << "</strong><br>";
                html << "<em>Описание:</em> " << p.content << "<br>";
                if (!p.secret.empty()) {
                    html << "<em>Секрет:</em> " << p.secret << "<br>";
                }
                html << "<a href='/view?id=" << p.id << "'>Посмотреть</a>";
                html << "</li><br>";
            }
            html << "</ul>";
        }

        html << "<h3>Опубликовать новую статью</h3>";
        html << "<form method='POST' action='/post'>";
        html << "<p><label>Название:<br><input name='title' required style='width:100%; padding:6px;'></label></p>";
        html << "<p><label>URL изображения:<br><input name='image_url' style='width:100%; padding:6px;' placeholder='https://example.com/image.jpg'></label></p>";
        html << "<p><label>Описание (публичное):<br><textarea name='content' rows='5' style='width:100%; padding:6px;' placeholder='Этот текст будет отображаться'></textarea></label></p>";
        html << "<p><label>Секретная информация (только для вас и админа):<br><textarea name='secret' rows='3' style='width:100%; padding:6px;' placeholder='Флаг или конфиденциальные данные'></textarea></label></p>";
        html << "<button type='submit' style='background:#0f3460; color:white; padding:8px 16px; border:none; border-radius:4px;'>Опубликовать</button>";
        html << "</form>";

        html << "<p style=\"margin-top:30px; text-align:center;\"><a href='/' style='color:#0f3460; text-decoration:underline;'>← Назад на главную</a></p>";
        html << "</div></body></html>";

        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    web_server.Post("/post", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_header_value("Cookie");
        if (token.substr(0, 8) == "session=") token = token.substr(8);
        if (g_active_tokens.find(token) == g_active_tokens.end()) {
            res.status = 403;
            res.set_content("Требуется вход", "text/plain; charset=utf-8");
            return;
        }
        int author_id = g_active_tokens[token];

        std::string title = req.get_param_value("title");
        std::string content = req.get_param_value("content");
        std::string secret = req.get_param_value("secret");
        if (title.empty() || content.empty()) {
            res.status = 400;
            res.set_content("Название и описание обязательны", "text/plain; charset=utf-8");
            return;
        }

        std::string display_image = "https://avatars.mds.yandex.net/i?id=75e8540704f46a2e09138e1e557bc2fb_l-4487962-images-thumbs&n=13.jpg";
        std::string provided_url = req.get_param_value("image_url");

        if (!provided_url.empty() && is_image_source_valid(provided_url)) {
            display_image = provided_url;
        }

        if (db_create_post(title, content, secret, display_image, author_id)) {
            res.set_redirect("/");
        } else {
            res.status = 500;
            res.set_content("Ошибка при публикации", "text/plain; charset=utf-8");
        }
    });

    web_server.Get("/view", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id_str = req.get_param_value("id");
        if (id_str.empty()) {
            res.status = 400;
            res.set_content("ID не указан", "text/plain; charset=utf-8");
            return;
        }
        int post_id = 0;
        try {
            post_id = std::stoi(id_str);
        } catch (...) {
            res.status = 400;
            res.set_content("Некорректный ID", "text/plain; charset=utf-8");
            return;
        }

        Post p = db_get_post_by_id(post_id);
        if (p.id == -1) {
            res.status = 404;
            res.set_content("Статья не найдена", "text/plain; charset=utf-8");
            return;
        }

        int current_user_id = -1;
        std::string token = req.get_header_value("Cookie");
        if (token.substr(0, 8) == "session=") {
            token = token.substr(8);
            if (g_active_tokens.find(token) != g_active_tokens.end()) {
                current_user_id = g_active_tokens.at(token);
            }
        }

        std::string metadata_result = fetch_remote_metadata(p.image_path, p.author_id, current_user_id);

        User author = db_get_user_by_id(p.author_id);
        bool can_access_private = (current_user_id == p.author_id || current_user_id == 1);

        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>" << p.title << "</title></head><body>";
        html << build_top_bar(token);

        html << "<div style='padding:20px; max-width:800px; margin:0 auto; font-family:Arial,sans-serif;'>";
        html << "<h1>" << p.title << "</h1>";
        html << "<div style=\"margin:20px 0; text-align:center;\">";
        html << "<img src=\"" << p.image_path << "\" style=\"max-width:100%; height:auto; border-radius:8px;\">";
        html << "</div>";
        html << "<h3>Описание:</h3>";
        html << "<div>" << p.content << "</div>";

        if (!metadata_result.empty()) {
            html << "<div style=\"background:#2d2d2d; color:#f8f8f2; padding:12px; border-radius:6px; margin:15px 0; font-family:monospace; white-space:pre-wrap;\">";
            html << "<strong>Доп. информация:</strong><br>";
            html << metadata_result;
            html << "</div>";
        }

        if (can_access_private && !p.secret.empty()) {
            html << "<div style=\"background:#fff8e1; padding:12px; margin-top:20px; border-left:4px solid #ffc107;\">";
            html << "<strong>🔒 Конфиденциальная информация:</strong><br>";
            html << p.secret;
            html << "</div>";
        }

        html << "<p style=\"margin-top:30px; padding-top:20px; border-top:1px solid #eee; color:#666;\">";
        html << "<em>Автор: " << author.username << "</em>";
        html << "</p>";
        html << "<p style=\"margin-top:20px; text-align:center;\">";
        html << "<a href='/' style='color:#0f3460; text-decoration:underline;'>← Назад на главную</a>";
        html << "</p>";
        html << "</div></body></html>";

        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    web_server.set_base_dir("./static");
    db_close();
    return 0;
}