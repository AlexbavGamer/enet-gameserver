#include "DatabaseManager.h"
#include "utils/Logger.h"

DatabaseManager::DatabaseManager() : worker_running_(false) {}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::connect(const std::string& connection_string) {
    try {
        connection_string_ = connection_string;

        // Cria sessão principal usando backend genérico
        sql_ = std::make_unique<soci::session>();

        ensureTablesExist();

        // Inicia worker thread para queries assíncronas
        worker_running_ = true;
        worker_thread_ = std::thread(&DatabaseManager::workerThread, this);

        Logger::info("Database connected successfully");
        return true;
    } catch (const soci::soci_error& e) {
        Logger::error("Database connection error: " + std::string(e.what()));
        return false;
    }
}

void DatabaseManager::disconnect() {
    if (worker_running_) {
        worker_running_ = false;
        queue_cv_.notify_all();

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    sql_.reset();
    Logger::info("Database disconnected");
}

void DatabaseManager::ensureTablesExist() {
    try {
        *sql_ << R"(
            CREATE TABLE IF NOT EXISTS players (
                id BIGINT AUTO_INCREMENT PRIMARY KEY,
                username VARCHAR(64) UNIQUE NOT NULL,
                password_hash VARCHAR(128) NOT NULL,
                level INT DEFAULT 1,
                health INT DEFAULT 100,
                pos_x DOUBLE DEFAULT 0.0,
                pos_y DOUBLE DEFAULT 0.0,
                pos_z DOUBLE DEFAULT 0.0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_login TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
            )
        )";

        *sql_ << R"(
            CREATE TABLE IF NOT EXISTS inventory (
                id BIGINT AUTO_INCREMENT PRIMARY KEY,
                player_id BIGINT,
                item_id INT NOT NULL,
                quantity INT DEFAULT 1,
                slot_index INT,
                UNIQUE(player_id, slot_index),
                FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
            )
        )";

        *sql_ << R"(
            CREATE TABLE IF NOT EXISTS sessions (
                id BIGINT AUTO_INCREMENT PRIMARY KEY,
                player_id BIGINT,
                session_token VARCHAR(256) UNIQUE NOT NULL,
                ip_address VARCHAR(45),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                expires_at TIMESTAMP NULL,
                FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE
            )
        )";

        Logger::info("Database tables verified/created");
    } catch (const soci::soci_error& e) {
        Logger::error("Table creation error: " + std::string(e.what()));
    }
}

std::optional<PlayerData> DatabaseManager::getPlayerByUsername(const std::string& username) {
    try {
        PlayerData player;
        *sql_ << "SELECT id, username, password_hash, level, health, pos_x, pos_y, pos_z "
                 "FROM players WHERE username = :username",
            soci::into(player.id),
            soci::into(player.username),
            soci::into(player.password_hash),
            soci::into(player.level),
            soci::into(player.health),
            soci::into(player.pos_x),
            soci::into(player.pos_y),
            soci::into(player.pos_z),
            soci::use(username);

        if (sql_->got_data()) {
            return player;
        }
        return std::nullopt;
    } catch (const soci::soci_error& e) {
        Logger::error("Query error: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DatabaseManager::createPlayer(const PlayerData& player) {
    try {
        *sql_ << "INSERT INTO players (username, password_hash, level, health, pos_x, pos_y, pos_z) "
                 "VALUES (:username, :password_hash, :level, :health, :pos_x, :pos_y, :pos_z)",
            soci::use(player.username),
            soci::use(player.password_hash),
            soci::use(player.level),
            soci::use(player.health),
            soci::use(player.pos_x),
            soci::use(player.pos_y),
            soci::use(player.pos_z);

        Logger::info("Player created: " + player.username);
        return true;
    } catch (const soci::soci_error& e) {
        Logger::error("Player creation error: " + std::string(e.what()));
        return false;
    }
}

bool DatabaseManager::updatePlayerPosition(uint64_t player_id, double x, double y, double z) {
    try {
        *sql_ << "UPDATE players SET pos_x = :x, pos_y = :y, pos_z = :z WHERE id = :id",
            soci::use(x), soci::use(y), soci::use(z), soci::use(player_id);
        return true;
    } catch (const soci::soci_error& e) {
        Logger::error("Position update error: " + std::string(e.what()));
        return false;
    }
}

bool DatabaseManager::updatePlayerStats(uint64_t player_id, int level, int health) {
    try {
        *sql_ << "UPDATE players SET level = :level, health = :health WHERE id = :id",
            soci::use(level), soci::use(health), soci::use(player_id);
        return true;
    } catch (const soci::soci_error& e) {
        Logger::error("Stats update error: " + std::string(e.what()));
        return false;
    }
}

// ======== Async operations ========

std::future<std::optional<PlayerData>> DatabaseManager::getPlayerByUsernameAsync(const std::string& username) {
    auto promise = std::make_shared<std::promise<std::optional<PlayerData>>>();
    auto future = promise->get_future();

    Task task;
    task.func = [this, username, promise]() {
        try {
            soci::session sql;
            PlayerData player;
            sql << "SELECT id, username, password_hash, level, health, pos_x, pos_y, pos_z "
                   "FROM players WHERE username = :username",
                soci::into(player.id),
                soci::into(player.username),
                soci::into(player.password_hash),
                soci::into(player.level),
                soci::into(player.health),
                soci::into(player.pos_x),
                soci::into(player.pos_y),
                soci::into(player.pos_z),
                soci::use(username);

            if (sql.got_data()) {
                promise->set_value(player);
            } else {
                promise->set_value(std::nullopt);
            }
        } catch (const std::exception& e) {
            Logger::error("Async query error: " + std::string(e.what()));
            promise->set_value(std::nullopt);
        }
    };

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();

    return future;
}

std::future<bool> DatabaseManager::updatePlayerPositionAsync(uint64_t player_id, double x, double y, double z) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    Task task;
    task.func = [this, player_id, x, y, z, promise]() {
        try {
            soci::session sql;
            sql << "UPDATE players SET pos_x = :x, pos_y = :y, pos_z = :z WHERE id = :id",
                soci::use(x), soci::use(y), soci::use(z), soci::use(player_id);
            promise->set_value(true);
        } catch (const std::exception& e) {
            Logger::error("Async update error: " + std::string(e.what()));
            promise->set_value(false);
        }
    };

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();

    return future;
}

void DatabaseManager::workerThread() {
    Logger::info("Database worker thread started");

    while (worker_running_) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() { return !task_queue_.empty() || !worker_running_; });

            if (!worker_running_ && task_queue_.empty()) break;

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }

        if (task.func) task.func();
    }

    Logger::info("Database worker thread stopped");
}

std::vector<std::unordered_map<std::string, std::string>> DatabaseManager::executeQuery(const std::string& query) {
    std::vector<std::unordered_map<std::string, std::string>> results;

    try {
        soci::rowset<soci::row> rs = (sql_->prepare << query);

        for (auto& row : rs) {
            std::unordered_map<std::string, std::string> row_data;

            for (std::size_t i = 0; i < row.size(); ++i) {
                const soci::column_properties& props = row.get_properties(i);
                std::string value;

                switch (row.get_indicator(i)) {
                    case soci::i_ok:
                        switch (props.get_data_type()) {
                            case soci::dt_string:
                                value = row.get<std::string>(i);
                                break;
                            case soci::dt_integer:
                                value = std::to_string(row.get<int>(i));
                                break;
                            case soci::dt_long_long:
                                value = std::to_string(row.get<long long>(i));
                                break;
                            case soci::dt_double:
                                value = std::to_string(row.get<double>(i));
                                break;
                            default:
                                value = "unsupported_type";
                        }
                        break;
                    case soci::i_null:
                        value = "NULL";
                        break;
                    default:
                        value = "error";
                }

                row_data[props.get_name()] = value;
            }

            results.push_back(row_data);
        }
    } catch (const soci::soci_error& e) {
        Logger::error("Query execution error: " + std::string(e.what()));
    }

    return results;
}
