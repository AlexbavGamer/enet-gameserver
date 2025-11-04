/**
 * @file logger.h
 * @brief Sistema de logging centralizado do servidor
 *
 * Este arquivo implementa um sistema de logging singleton que padroniza
 * todas as mensagens de log do sistema. O suporta diferentes níveis de
 * severidade (DEBUG, INFO, WARNING, ERROR) e formatação consistente.
 *
 * Principais características:
 * - Padrão Singleton para garantir única instância
 * - Vários níveis de log com formatação visual
 * - Macros para facilitar o uso
 * - Compatibilidade com código legado
 *
 * @author Sistema de Servidor Seguro
 * @version 1.0
 * @date 2025
 */

#ifdef ERROR
#undef ERROR
#endif

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @class Logger
 * @brief Sistema de logging centralizado do servidor
 *
 * Esta classe implementa um padrão Singleton para fornecer um sistema
 * de logging consistente em todo o servidor. Ela suporta diferentes níveis
 * de severidade e formatação visual padronizada.
 *
 * Os níveis de log são:
 * - DEBUG: Informações detalhadas para depuração
 * - INFO: Informações gerais de operação normal
 * - WARNING: Advertências que não impedem o funcionamento
 * - ERROR: Erros que precisam de atenção
 */
class Logger {
public:
    /**
     * @enum Level
     * @brief Níveis de severidade do log
     */
    enum class Level {
        DEBUG,   ///< Nível de depuração
        INFO,    ///< Nível informativo
        WARNING, ///< Nível de advertência
        ERROR    ///< Nível de erro
    };

    /**
     * @brief Obtém a instância singleton do Logger
     * @return Referência para a instância única do Logger
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Registra uma mensagem de log
     * @param level Nível de severidade da mensagem
     * @param prefix Prefixo identificador da origem da mensagem
     * @param message Conteúdo da mensagem de log
     */
    void log(Level level, const std::string& prefix, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::string levelStr;
        switch (level) {
            case Level::DEBUG:   levelStr = "DEBUG"; break;
            case Level::INFO:    levelStr = "INFO"; break;
            case Level::WARNING: levelStr = "WARN"; break;
            case Level::ERROR:   levelStr = "ERROR"; break;
        }

        std::cout << "[" << levelStr << "] " << prefix << " " << message << std::endl;
    }

    /**
     * @brief Registra mensagem de nível DEBUG
     * @param prefix Prefixo identificador
     * @param message Conteúdo da mensagem
     */
    void debug(const std::string& prefix, const std::string& message) {
        log(Level::DEBUG, prefix, message);
    }

    /**
     * @brief Registra mensagem de nível INFO
     * @param prefix Prefixo identificador
     * @param message Conteúdo da mensagem
     */
    void info(const std::string& prefix, const std::string& message) {
        log(Level::INFO, prefix, message);
    }

    /**
     * @brief Registra mensagem de nível WARNING
     * @param prefix Prefixo identificador
     * @param message Conteúdo da mensagem
     */
    void warning(const std::string& prefix, const std::string& message) {
        log(Level::WARNING, prefix, message);
    }

    /**
     * @brief Registra mensagem de nível ERROR
     * @param prefix Prefixo identificador
     * @param message Conteúdo da mensagem
     */
    void error(const std::string& prefix, const std::string& message) {
        log(Level::ERROR, prefix, message);
    }

    /**
     * @brief Função de conveniência para compatibilidade com código legado
     * @param message Mensagem a ser impressa
     */
    void safePrint(const std::string& message) {
        info("", message);
    }

    /**
     * @brief Função de conveniência para compatibilidade com código legado
     * @param prefix Prefixo identificador
     * @param message Mensagem de erro a ser impressa
     */
    void safePrintError(const std::string& prefix, const std::string& message) {
        error(prefix, message);
    }

private:
    Logger() = default;                    ///< Construtor privado
    ~Logger() = default;                   ///< Destrutor privado
    Logger(const Logger&) = delete;        ///< Construtor de cópia deletado
    Logger& operator=(const Logger&) = delete; ///< Operador de atribuição deletado
};

// Macros para facilitar o uso do sistema de logging
#define LOG_DEBUG(prefix, message) Logger::getInstance().debug(prefix, message)  ///< Macro para log de debug
#define LOG_INFO(prefix, message) Logger::getInstance().info(prefix, message)    ///< Macro para log informativo
#define LOG_WARNING(prefix, message) Logger::getInstance().warning(prefix, message) ///< Macro para log de advertência
#define LOG_ERROR(prefix, message) Logger::getInstance().error(prefix, message)  ///< Macro para log de erro
#define SAFE_PRINT(message) Logger::getInstance().safePrint(message)              ///< Macro para impressão segura (compatibilidade)

#endif // LOGGER_H