#pragma once
#include <soci/soci.h>
#include <soci/mysql/soci-mysql.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <any>
#include <iostream>
#include <set>
#include <regex>

using json = nlohmann::json;

// Classe segura para operações de banco de dados
class SecureDatabase {
public:
    SecureDatabase(const std::string& conn_str = "db=game_db user=root host=127.0.0.1 port=3306");

    // CREATE: Insere um registro com validação de segurança
    bool create(const std::string& table, const json& data);

    // READ: Busca por chave primária (ou WHERE custom)
    std::optional<json> read(const std::string& table, const std::string& where = "");

    // READ ALL: Todos os registros
    std::vector<json> readAll(const std::string& table, const std::string& where = "");

    // UPDATE: Atualiza com WHERE
    bool update(const std::string& table, const json& data, const std::string& where);

    // DELETE: Remove com WHERE
    bool remove(const std::string& table, const std::string& where);

    // Métodos de validação
    bool isValidTableName(const std::string& table);
    bool isValidColumnName(const std::string& column);
    bool sanitizeInput(std::string& input);
    bool sanitizeInput(const std::string& input);

private:
    soci::session sql_;
    std::set<std::string> allowed_tables_;
    std::set<std::string> allowed_columns_;
    std::regex table_name_regex_;
    std::regex column_name_regex_;

    // Converte tipo C++ → SQL com segurança
    void bindValues(soci::statement& st, const std::vector<std::pair<std::string, std::any>>& values);

    // Métodos internos
    std::string buildSafeQuery(const std::string& operation, const std::string& table, 
                              const json& data = {}, const std::string& where = "");
    
    // Validação de nomes de tabelas e colunas
    bool validateIdentifier(const std::string& identifier, const std::regex& pattern);
    
    // Inicializa lista de tabelas permitidas
    void initializeAllowedTables();
};