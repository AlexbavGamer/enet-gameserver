-- Game Logic Script para Lua
-- Este script demonstra como usar o sistema Lua integrado ao servidor

-- Função principal chamada quando o servidor inicia
function main()
    print("Sistema de lógica do jogo inicializado via Lua!")
    log("Carregado script de lógica do jogo")
end

-- Função chamada quando um jogador se conecta
function onPlayerConnect(playerId, username)
    log("Jogador " .. username .. " (ID: " .. playerId .. ") conectado")
    
    -- Enviar mensagem de boas-vindas personalizada
    local welcomeMsg = {
        user = "Sistema",
        msg = "Bem-vindo " .. username .. "! Use /ajuda para comandos disponíveis."
    }
    
    -- Enviar mensagem para o jogador específico
    -- Em uma implementação real, você chamaria uma função C++ para enviar pacotes
    print("Enviando mensagem de boas-vindas para " .. username)
end

-- Função chamada quando um jogador se desconecta
function onPlayerDisconnect(playerId, username)
    log("Jogador " .. username .. " (ID: " .. playerId .. ") desconectado")
    
    -- Notificar outros jogadores
    local disconnectMsg = {
        user = "Sistema",
        msg = username .. " saiu do jogo."
    }
    
    print("Notificando sobre desconexão de " .. username)
end

-- Função chamada quando um jogador se move
function onPlayerMove(playerId, username, x, y)
    -- Implementar lógica de movimento personalizada
    -- Por exemplo: verificar colisões, atualizar estado do mundo, etc.
    
    -- Exemplo: registrar movimento no log
    log(string.format("Jogador %s (ID: %d) moveu para (%.1f, %.1f)", username, playerId, x, y))
    
    -- Exemplo: verificar se jogador atingiu um checkpoint
    if x > 100 and y > 100 then
        local checkpointMsg = {
            user = "Sistema",
            msg = username .. " atingiu o checkpoint principal!"
        }
        print("Checkpoint atingido por " .. username)
    end
end

-- Função chamada quando uma mensagem de chat é recebida
function onChatMessage(playerId, username, message)
    -- Processar comandos especiais
    if message:sub(1, 1) == "/" then
        return processCommand(playerId, username, message)
    end
    
    -- Log de mensagem normal
    log("Chat: " .. username .. ": " .. message)
    
    -- Exemplo: filtrar palavras inadequadas
    local filteredMessage = filterInappropriateWords(message)
    if filteredMessage ~= message then
        local warningMsg = {
            user = "Sistema",
            msg = username .. ", sua mensagem foi filtrada por conter conteúdo inadequado."
        }
        print("Mensagem filtrada de " .. username)
    end
    
    return filteredMessage or message
end

-- Processar comandos de chat
function processCommand(playerId, username, command)
    local cmd = command:sub(2):match("^%s*(%S+)")
    local args = command:sub(#cmd + 3):gsub("^%s+", ""):gsub("%s+$", "")
    
    log("Comando recebido: " .. cmd .. " de " .. username)
    
    -- Comandos disponíveis
    if cmd == "ajuda" then
        return "Comandos disponíveis: /ajuda, /posicao, /jogadores, /tempo"
    elseif cmd == "posicao" then
        local player = getPlayerById(playerId)
        if player then
            return string.format("Sua posição atual: (%.1f, %.1f)", player.x, player.y)
        end
        return "Não foi possível obter sua posição"
    elseif cmd == "jogadores" then
        local count = getPlayerCount()
        return string.format("Jogadores online: %d", count)
    elseif cmd == "tempo" then
        return "Tempo de jogo: " .. getGameTime()
    elseif cmd == "ping" then
        return "Pong! " .. username
    else
        return "Comando '" .. cmd .. "' não reconhecido. Use /ajuda para ajuda."
    end
end

-- Funções utilitárias (simuladas)
function filterInappropriateWords(message)
    -- Implementar filtro de palavras
    local inappropriate = {"palavrão1", "palavrão2", "inadequado"}
    local filtered = message
    
    for _, word in ipairs(inappropriate) do
        filtered = filtered:gsub(word, "***", 1, true)
    end
    
    return filtered
end

function getPlayerById(playerId)
    -- Em implementação real, buscaria do gerenciador C++
    return {x = math.random(0, 200), y = math.random(0, 200)}
end

function getPlayerCount()
    -- Em implementação real, contaria jogadores ativos
    return math.random(1, 20)
end

function getGameTime()
    -- Em implementação real, obteria tempo de jogo
    local hours = os.date("%H")
    local minutes = os.date("%M")
    return string.format("%s:%s", hours, minutes)
end

-- Funções de evento que podem ser registradas pelo servidor C++
function registerEventHandlers()
    -- Em implementação real, registraria callbacks com o servidor
    log("Registrando handlers de eventos Lua")
end

-- Função de inicialização
function initialize()
    main()
    registerEventHandlers()
    
    -- Configurar variáveis globais
    _VERSION = "GameServer Lua v1.0"
    _DEBUG = true
    
    log("Sistema Lua inicializado com sucesso")
end

-- Chamar inicialização quando o script for carregado
initialize()