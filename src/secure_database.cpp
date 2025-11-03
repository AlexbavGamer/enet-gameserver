#include "secure_database.h"
#include <soci/soci.h>
#include <soci/mysql/soci-mysql.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>
#include <cctype>
#include "encoding_utils.h"

using json = nlohmann::json;

SecureDatabase::SecureDatabase(const std::string& conn_str) {
    try {
        sql_.open(soci::mysql, conn_str);

        initializeAllowedTables();

        // Compilar regex para validação de nomes
        table_name_regex_.assign("^[a-zA-Z_][a-zA-Z0-9_]*$");
        column_name_regex_.assign("^[a-zA-Z_][a-zA-Z0-9_]*$");

        safePrint("[SECURE DB] Banco de dados inicializado com segurança");
    } catch (const std::exception& e) {
        safePrint("[SECURE DB ERRO] " + std::string(e.what()));
    }
}

void SecureDatabase::initializeAllowedTables() {
    // Lista de tabelas permitidas - pode ser carregada do banco de dados
    allowed_tables_ = {"players", "accounts", "inventory", "chat_messages", "game_sessions"};
    
    // Lista de colunas permitidas por tabela
    allowed_columns_ = {
        "id", "username", "password", "email", "created_at", "updated_at",
        "player_id", "item_id", "quantity", "slot", "equipped",
        "session_id", "start_time", "end_time", "status"
    };
}

bool SecureDatabase::isValidTableName(const std::string& table) {
    return validateIdentifier(table, table_name_regex_) && 
           allowed_tables_.find(table) != allowed_tables_.end();
}

bool SecureDatabase::isValidColumnName(const std::string& column) {
    return validateIdentifier(column, column_name_regex_) && 
           allowed_columns_.find(column) != allowed_columns_.end();
}

bool SecureDatabase::validateIdentifier(const std::string& identifier, const std::regex& pattern) {
    return std::regex_match(identifier, pattern);
}

bool SecureDatabase::sanitizeInput(std::string& input) {
    // Remove caracteres perigosos
    input.erase(std::remove_if(input.begin(), input.end(), [](char c) {
        return !isalnum(c) && c != '_' && c != ' ' && c != '.' && c != '-';
    }), input.end());
    
    // Converte para minúsculas (opcional)
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    
    return true;
}

bool SecureDatabase::sanitizeInput(const std::string& input) {
    std::string temp = input;
    return sanitizeInput(temp);
}

std::string SecureDatabase::buildSafeQuery(const std::string& operation, const std::string& table, 
                                         const json& data, const std::string& where) {
    if (!isValidTableName(table)) {
        throw std::runtime_error("Tabela inválida ou não permitida: " + table);
    }

    std::string query;
    
    if (operation == "INSERT") {
        query = "INSERT INTO " + table + " (";
        std::string placeholders = " VALUES (";
        soci::values vals;

        for (auto& [key, val] : data.items()) {
            if (!isValidColumnName(key)) {
                throw std::runtime_error("Coluna inválida ou não permitida: " + key);
            }
            query += key + ", ";
            placeholders += ":" + key + ", ";
            
            if (val.is_string()) {
                vals.set(key, val.get<std::string>());
            } else if (val.is_number_integer()) {
                vals.set(key, val.get<int>());
            } else if (val.is_number_float()) {
                vals.set(key, val.get<double>());
            } else {
                vals.set(key, val.dump());
            }
        }
        
        if (data.empty()) {
            throw std::runtime_error("Nenhum dado fornecido para inserção");
        }
        
        query.pop_back(); query.pop_back();
        placeholders.pop_back(); placeholders.pop_back();
        query += ")" + placeholders + ")";
        
    } else if (operation == "UPDATE") {
        if (data.empty()) {
            throw std::runtime_error("Nenhum dado fornecido para atualização");
        }
        
        query = "UPDATE " + table + " SET ";
        for (auto& [key, val] : data.items()) {
            if (!isValidColumnName(key)) {
                throw std::runtime_error("Coluna inválida ou não permitida: " + key);
            }
            query += key + " = :" + key + ", ";
        }
        query.pop_back(); query.pop_back();
        
        if (!where.empty()) {
            std::string where_copy = where;
            sanitizeInput(where_copy);
            query += " WHERE " + where_copy;
        }
    } else if (operation == "DELETE") {
        query = "DELETE FROM " + table;
        if (!where.empty()) {
            std::string where_copy = where;
            sanitizeInput(where_copy);
            query += " WHERE " + where_copy;
        }
    } else if (operation == "SELECT") {
        query = "SELECT * FROM " + table;
        if (!where.empty()) {
            std::string where_copy = where;
            sanitizeInput(where_copy);
            query += " WHERE " + where_copy;
        }
    }
    
    return query;
}

// CREATE
bool SecureDatabase::create(const std::string& table, const json& data) {
    try {
        if (data.empty()) return false;

        safePrint("[SECURE DB] Criando registro na tabela: " + table);
        safePrint("[SECURE DB] Conteúdo dos dados: " + data.dump());

        std::string query = buildSafeQuery("INSERT", table, data);
        safePrint("[SECURE DB] Query SQL segura: " + query);
        
        soci::statement st(sql_);
        st.alloc();
        st.prepare(query);
        
        // Bind values de forma segura
        std::vector<std::pair<std::string, std::any>> values;
        for (auto& [key, val] : data.items()) {
            values.emplace_back(":" + key, val);
        }
        bindValues(st, values);
        
        st.execute(true);
        safePrint("[SECURE DB] Registro criado com sucesso");
        return true;
        
    } catch (const std::exception& e) {
        safePrint("[SECURE DB ERRO] " + std::string(e.what()));
        return false;
    }
}

// READ (WHERE opcional)
std::optional<json> SecureDatabase::read(const std::string& table, const std::string& where) {
    try {
        std::string query = buildSafeQuery("SELECT", table, {}, where);
        soci::rowset<soci::row> rs = sql_.prepare << query;
        auto it = rs.begin();
        if (it == rs.end()) return std::nullopt;

        json result;
        const soci::row& row = *it;
        for (std::size_t i = 0; i < row.size(); ++i) {
            const soci::column_properties& props = row.get_properties(i);
            std::string col_name = props.get_name();
            const auto& indicator = row.get_indicator(i);

            if (indicator == soci::i_null) {
                result[col_name] = nullptr;
            } else if (indicator == soci::i_ok) {
                switch (props.get_data_type()) {
                    case soci::dt_string: result[col_name] = row.get<std::string>(i); break;
                    case soci::dt_integer: result[col_name] = row.get<int>(i); break;
                    case soci::dt_double: result[col_name] = row.get<double>(i); break;
                    case soci::dt_date: {
                        std::tm t = row.get<std::tm>(i);
                        char buffer[20];
                        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &t);
                        result[col_name] = buffer;
                        break;
                    }
                    default: result[col_name] = "unknown_type";
                }
            }
        }
        return result;
    } catch (const std::exception& e) {
        safePrint("[SECURE DB READ ERRO] " + std::string(e.what()));
        return std::nullopt;
    }
}

// READ ALL
std::vector<json> SecureDatabase::readAll(const std::string& table, const std::string& where) {
    std::vector<json> results;
    try {
        std::string query = buildSafeQuery("SELECT", table, {}, where);
        soci::rowset<soci::row> rs = sql_.prepare << query;
        for (const auto& row : rs) {
            json obj;
            for (std::size_t i = 0; i < row.size(); ++i) {
                const soci::column_properties& props = row.get_properties(i);
                std::string col_name = props.get_name();
                const auto& indicator = row.get_indicator(i);

                if (indicator == soci::i_null) {
                    obj[col_name] = nullptr;
                } else {
                    switch (props.get_data_type()) {
                        case soci::dt_string: obj[col_name] = row.get<std::string>(i); break;
                        case soci::dt_integer: obj[col_name] = row.get<int>(i); break;
                        case soci::dt_double: obj[col_name] = row.get<double>(i); break;
                        case soci::dt_date: {
                            std::tm t = row.get<std::tm>(i);
                            char buffer[20];
                            strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &t);
                            obj[col_name] = buffer;
                            break;
                        }
                        default: obj[col_name] = "unknown";
                    }
                }
            }
            results.push_back(obj);
        }
    } catch (const std::exception& e) {
        safePrint("[SECURE DB READALL ERRO] " + std::string(e.what()));
    }
    return results;
}

// UPDATE
bool SecureDatabase::update(const std::string& table, const json& data, const std::string& where) {
    try {
        if (data.empty() || where.empty()) return false;

        std::string query = buildSafeQuery("UPDATE", table, data, where);
        safePrint("[SECURE DB] Query UPDATE segura: " + query);
        
        soci::statement st(sql_);
        st.alloc();
        st.prepare(query);
        
        // Bind values de forma segura
        std::vector<std::pair<std::string, std::any>> values;
        for (auto& [key, val] : data.items()) {
            values.emplace_back(":" + key, val);
        }
        bindValues(st, values);
        
        st.execute(true);
        safePrint("[SECURE DB UPDATE] " + table + " OK");
        return true;
    } catch (const std::exception& e) {
        safePrint("[SECURE DB UPDATE ERRO] " + std::string(e.what()));
        return false;
    }
}

// DELETE
bool SecureDatabase::remove(const std::string& table, const std::string& where) {
    try {
        if (where.empty()) return false;
        
        std::string query = buildSafeQuery("DELETE", table, {}, where);
        safePrint("[SECURE DB] Query DELETE segura: " + query);
        
        sql_ << query;
        safePrint("[SECURE DB DELETE] " + table + " OK");
        return true;
    } catch (const std::exception& e) {
        safePrint("[SECURE DB DELETE ERRO] " + std::string(e.what()));
        return false;
    }
}

void SecureDatabase::bindValues(soci::statement& st, const std::vector<std::pair<std::string, std::any>>& params) {
    for (const auto& [name, val] : params) {
        if (val.type() == typeid(std::string)) {
            std::string str_val = std::any_cast<std::string>(val);
            st.exchange(soci::use(str_val, name));
        } 
        else if (val.type() == typeid(int)) {
            int int_val = std::any_cast<int>(val);
            st.exchange(soci::use(int_val, name));
        } 
        else if (val.type() == typeid(double)) {
            double dbl_val = std::any_cast<double>(val);
            st.exchange(soci::use(dbl_val, name));
        } 
        else if (val.type() == typeid(float)) {
            double dbl_val = static_cast<double>(std::any_cast<float>(val));
            st.exchange(soci::use(dbl_val, name));
        }
    }
}