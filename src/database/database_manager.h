#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include "../secure_database.h"
#include <memory>
#include <string>

class DatabaseManager {
private:
    std::unique_ptr<SecureDatabase> database_;
    
public:
    DatabaseManager();
    ~DatabaseManager() = default;
    
    // Inicializa o banco de dados
    bool initialize();
    
    // Verifica se o banco de dados está válido
    bool isValid() const;
    
    // Operações de jogador
    bool createPlayer(const std::string& username, float x, float y);
    bool updatePlayerPosition(int playerId, float x, float y);
    bool removePlayer(int playerId);
    
    // Métodos de acesso ao banco de dados original
    SecureDatabase* getDatabase() const { return database_.get(); }
    
private:
    // Valida se o nome da tabela é válido
    bool isValidTableName(const std::string& tableName) const;
};

#endif // DATABASE_MANAGER_H