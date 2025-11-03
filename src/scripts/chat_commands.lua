-- Chat Commands Script para Lua
-- Este script gerencia comandos de chat avançados

-- Variáveis de estado
local playerStats = {} -- Armazena estatísticas dos jogadores
local chatHistory = {} -- Histórico de chat
local commandCooldowns = {} -- Controle de cooldown para comandos

-- Função principal de inicialização
function main()
    print("Sistema de comandos de chat inicializado via Lua!")
    log("INFO", "Carregado script de comandos de chat")
    
    -- Inicializar estruturas de dados
    initializeChatSystem()
end

-- Inicializar sistema de chat
function initializeChatSystem()
    config = {}
    
    -- Configurar limite de histórico
    config.maxHistory = 100
    
    -- Configurar cooldowns (em segundos)
    config.commandCooldowns = {
        ["roll"] = 5,
        ["dado"] = 3,
        ["moeda"] = 2,
        ["sorteio"] = 30
    }
    
    log("INFO", "Sistema de chat inicializado com sucesso")
end

-- Processar comandos de chat avançados
function processAdvancedCommand(playerId, username, command)
    local cmd = command:sub(2):match("^%s*(%S+)")
    local args = command:sub(#cmd + 3):gsub("^%s+", ""):gsub("%s+$", "")
    
    log("DEBUG", "Comando avançado recebido: " .. cmd .. " de " .. username)
    
    -- Verificar cooldown
    if isOnCooldown(playerId, cmd) then
        local remaining = getRemainingCooldown(playerId, cmd)
        return "Aguarde " .. remaining .. " segundos antes de usar o comando /" .. cmd .. " novamente."
    end
    
    -- Processar comandos específicos
    if cmd == "roll" then
        return handleRollCommand(playerId, username, args)
    elseif cmd == "dado" then
        return handleDiceCommand(playerId, username, args)
    elseif cmd == "moeda" then
        return handleCoinCommand(playerId, username)
    elseif cmd == "sorteio" then
        return handleLotteryCommand(playerId, username, args)
    elseif cmd == "estatisticas" then
        return handleStatsCommand(playerId, username)
    elseif cmd == "historico" then
        return handleHistoryCommand(playerId, username, args)
    elseif cmd == "limpar" then
        return handleClearCommand(playerId, username, args)
    elseif cmd == "ajuda" then
        return handleHelpCommand(playerId, username)
    else
        return "Comando '" .. cmd .. "' não reconhecido. Use /ajuda para ver comandos disponíveis."
    end
end

-- Comando /roll - Rolar um dado personalizado
function handleRollCommand(playerId, username, args)
    -- Parsear argumentos: /roll [faces] [quantidade]
    local faces = tonumber(args:match("(%d+)")) or 6
    local quantity = tonumber(args:match("%s+(%d+)$")) or 1
    
    -- Validar entrada
    if faces < 1 or faces > 1000 then
        return "Número de faces inválido. Use entre 1 e 1000."
    end
    
    if quantity < 1 or quantity > 10 then
        return "Quantidade inválida. Use entre 1 e 10 dados."
    end
    
    -- Realizar rolagens
    local results = {}
    local total = 0
    
    for i = 1, quantity do
        local roll = math.random(1, faces)
        table.insert(results, roll)
        total = total + roll
    end
    
    -- Formatar mensagem
    local resultText = username .. " rolou " .. quantity .. "d" .. faces .. ": "
    for i, roll in ipairs(results) do
        resultText = resultText .. roll
        if i < #results then
            resultText = resultText .. ", "
        end
    end
    
    if quantity > 1 then
        resultText = resultText .. " (Total: " .. total .. ")"
    end
    
    -- Adicionar cooldown
    setCooldown(playerId, "roll", config.commandCooldowns["roll"])
    
    return resultText
end

-- Comando /dado - Rolar um dado de 6 faces
function handleDiceCommand(playerId, username, args)
    local roll = math.random(1, 6)
    
    -- Adicionar cooldown
    setCooldown(playerId, "dado", config.commandCooldowns["dado"])
    
    return username .. " rolou um dado e tirou: " .. roll
end

-- Comando /moeda - Jogar uma moeda
function handleCoinCommand(playerId, username)
    local result = math.random(1, 2) == 1 and "CARA" or "COROA"
    
    -- Adicionar cooldown
    setCooldown(playerId, "moeda", config.commandCooldowns["moeda"])
    
    return username .. " jogou uma moeda e deu: " .. result
end

-- Comando /sorteio - Sistema de sorteio simples
function handleLotteryCommand(playerId, username, args)
    -- Verificar se já está participando de um sorteio
    if playerStats[playerId] and playerStats[playerId].lottery then
        return username .. ", você já está participando do sorteio atual!"
    end
    
    -- Registrar participação
    if not playerStats[playerId] then
        playerStats[playerId] = {lottery = true}
    else
        playerStats[playerId].lottery = true
    end
    
    -- Adicionar cooldown
    setCooldown(playerId, "sorteio", config.commandCooldowns["sorteio"])
    
    return username .. " entrou no sorteio! Use /sorteio novamente para sair."
end

-- Comando /estatisticas - Mostrar estatísticas do jogador
function handleStatsCommand(playerId, username)
    if not playerStats[playerId] then
        playerStats[playerId] = {
            messages = 0,
            commands = 0,
            rolls = 0,
            lottery = false
        }
    end
    
    local stats = playerStats[playerId]
    local statsText = username .. " - Estatísticas:\n"
    statsText = statsText .. "  Mensagens enviadas: " .. (stats.messages or 0) .. "\n"
    statsText = statsText .. "  Comandos usados: " .. (stats.commands or 0) .. "\n"
    statsText = statsText .. "  Rolagens de dado: " .. (stats.rolls or 0) .. "\n"
    statsText = statsText .. "  Participando do sorteio: " .. (stats.lottery and "Sim" or "Não")
    
    return statsText
end

-- Comando /historico - Mostrar histórico de chat
function handleHistoryCommand(playerId, username, args)
    local limit = tonumber(args) or 5
    limit = math.max(1, math.min(limit, 20)) -- Limitar entre 1 e 20
    
    if #chatHistory == 0 then
        return "Nenhuma mensagem no histórico de chat."
    end
    
    local startIndex = math.max(1, #chatHistory - limit + 1)
    local endIndex = #chatHistory
    
    local historyText = "Últimas " .. limit .. " mensagens:\n"
    for i = startIndex, endIndex do
        historyText = historyText .. chatHistory[i] .. "\n"
    end
    
    return historyText
end

-- Comando /limpar - Limpar histórico ou estatísticas
function handleClearCommand(playerId, username, args)
    local target = args:lower()
    
    if target == "historico" then
        chatHistory = {}
        return "Histórico de chat limpo por " .. username .. "."
    elseif target == "estatisticas" then
        playerStats[playerId] = {
            messages = 0,
            commands = 0,
            rolls = 0,
            lottery = false
        }
        return "Estatísticas limpas por " .. username .. "."
    else
        return "Uso: /limpar [historico|estatisticas]"
    end
end

-- Comando /ajuda - Mostrar ajuda de comandos avançados
function handleHelpCommand(playerId, username)
    local helpText = username .. ", comandos avançados disponíveis:\n"
    helpText = helpText .. "/roll [faces] [quantidade] - Rolar dados personalizados\n"
    helpText = helpText .. "/dado - Rolar um dado de 6 faces\n"
    helpText = helpText .. "/moeda - Jogar uma moeda\n"
    helpText = helpText .. "/sorteio - Entrar/sair do sorteio\n"
    helpText = helpText .. "/estatisticas - Ver suas estatísticas\n"
    helpText = helpText .. "/historico [quantidade] - Ver histórico de chat\n"
    helpText = helpText .. "/limpar [historico|estatisticas] - Limpar dados\n"
    helpText = helpText .. "\nCooldowns aplicados a cada comando."
    
    return helpText
end

-- Sistema de cooldown
function setCooldown(playerId, command, seconds)
    if not commandCooldowns[playerId] then
        commandCooldowns[playerId] = {}
    end
    
    commandCooldowns[playerId][command] = os.time() + seconds
end

function isOnCooldown(playerId, command)
    if not commandCooldowns[playerId] or not commandCooldowns[playerId][command] then
        return false
    end
    
    return os.time() < commandCooldowns[playerId][command]
end

function getRemainingCooldown(playerId, command)
    if not commandCooldowns[playerId] or not commandCooldowns[playerId][command] then
        return 0
    end
    
    local remaining = commandCooldowns[playerId][command] - os.time()
    return math.max(0, remaining)
end

-- Sistema de histórico de chat
function addToHistory(message)
    table.insert(chatHistory, message)
    
    -- Manter tamanho máximo
    if #chatHistory > config.maxHistory then
        table.remove(chatHistory, 1)
    end
end

-- Atualizar estatísticas do jogador
function updatePlayerStats(playerId, action)
    if not playerStats[playerId] then
        playerStats[playerId] = {
            messages = 0,
            commands = 0,
            rolls = 0,
            lottery = false
        }
    end
    
    if action == "message" then
        playerStats[playerId].messages = (playerStats[playerId].messages or 0) + 1
    elseif action == "command" then
        playerStats[playerId].commands = (playerStats[playerId].commands or 0) + 1
    elseif action == "roll" then
        playerStats[playerId].rolls = (playerStats[playerId].rolls or 0) + 1
    end
end

-- Função de inicialização
function initialize()
    main()
    
    -- Exportar funções para o servidor C++
    -- Em implementação real, estas seriam registradas como callbacks
    log("INFO", "Sistema de comandos de chat inicializado com sucesso")
end

-- Chamar inicialização quando o script for carregado
initialize()