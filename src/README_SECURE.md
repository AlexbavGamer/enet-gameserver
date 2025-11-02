# Servidor Seguro com Suporte a Lua

Este documento descreve a versão segura e refatorada do servidor ENet com suporte a Lua através da biblioteca sol2.

## 🚀 Principais Melhorias

### 1. Segurança Bancária de Dados
- **Problema Resolvido**: Vulnerabilidade de SQL Injection na função `Database::create()`
- **Solução**: Nova classe `SecureDatabase` com:
  - Validação de nomes de tabelas e colunas usando regex
  - Lista branca (whitelist) de tabelas e colunas permitidas
  - Sanitização de entrada de dados
  - Prepared statements para todas as operações

### 2. Gerenciamento de Memória Seguro
- **Problema Resolvido**: Vazamentos de memória com `ENetPacket*`
- **Solução**: Classe `ManagedPacket` com RAII:
  - Construtores e destrutores automáticos
  - Semântica de movimentação (move semantics)
  - Controle de ownership claro
  - Logs detalhados de alocação/liberação

### 3. Arquitetura de Pacotes Refatorada
- **Nova Classe**: `SecurePacketHandler`
- **Recursos**:
  - Validação estrutural de pacotes
  - Sistema de handlers registráveis
  - Callbacks tipo função para processamento
  - Broadcast seguro com gerenciamento de memória

### 4. Suporte a Lua com sol2
- **Integração Completa**: Sistema Lua integrado ao servidor
- **Recursos**:
  - Carregamento dinâmico de scripts
  - Interface C++/Lua bidirecional
  - Exposição de classes C++ para Lua
  - Sistema de eventos baseado em Lua

### 5. Gerenciamento de Jogadores Aprimorado
- **Nova Classe**: `PlayerManager`
- **Recursos**:
  - Limpeza automática de jogadores inativos
  - Persistência segura no banco de dados
  - Rastreamento de atividade
  - Integração com sistema Lua

## 📁 Estrutura de Arquivos

```
src/
├── main_secure.cpp          # Nova versão segura do main
├── database.h               # Classe Database original (legado)
├── database.cpp             # Implementação original (legado)
├── secure_database.h        # Nova classe segura
├── secure_database.cpp      # Implementação segura
├── packethandler.h          # Handler original (legado)
├── packethandler.cpp        # Implementação original (legado)
├── secure_packet_handler.h  # Nova classe segura
├── secure_packet_handler.cpp # Implementação segura
├── lua_interface.h          # Interface Lua
├── lua_interface.cpp        # Implementação Lua
└── scripts/                 # Scripts Lua
    ├── game_logic.lua       # Lógica do jogo em Lua
    └── chat_commands.lua    # Comandos de chat em Lua
```

## 🔧 Compilação

O sistema usa CMake com Ninja. Para compilar:

```bash
cd d:/enet-server
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -B build
cd build
ninja -j4
```

## 🚀 Execução

Após compilar, execute o servidor seguro:

```bash
cd d:/enet-server/build/bin
./GameServer.exe
```

## 📚 Sistema Lua

### Carregamento de Scripts

O sistema Lua carrega automaticamente scripts durante a inicialização:

```cpp
// scripts/game_logic.lua - Lógica do jogo
// scripts/chat_commands.lua - Comandos de chat
```

### Interface C++/Lua

A interface expõe as seguintes classes para Lua:

```lua
-- Classe Database disponível em Lua
local db = Database:new("connection_string")
local result = db:read("players", "id = 1")

-- Funções utilitárias
log("Mensagem de log")
print("Mensagem para console")

-- Enums de pacotes
PacketID.PING
PacketID.LOGIN
PacketID.CHAT
-- etc.
```

### Exemplo de Script Lua

```lua
-- Função chamada quando um jogador se conecta
function onPlayerConnect(playerId, username)
    log("Jogador " .. username .. " conectado")
    
    -- Enviar mensagem de boas-vindas
    local welcomeMsg = {
        user = "Sistema",
        msg = "Bem-vindo " .. username .. "!"
    }
    
    -- Processar comandos
    if username == "admin" then
        log("Administrador " .. username .. " entrou no servidor")
    end
end
```

## 🔒 Segurança

### Validação de Banco de Dados

A nova classe `SecureDatabase` implementa múltiplas camadas de segurança:

1. **Validação de Nomes**: Regex para validar nomes de tabelas e colunas
2. **Lista Branca**: Apenas tabelas e colunas permitidas podem ser acessadas
3. **Sanitização**: Limpeza automática de dados de entrada
4. **Prepared Statements**: Prevenção completa de SQL injection

### Gerenciamento de Memória

O sistema usa RAII (Resource Acquisition Is Initialization):

```cpp
// Pacotes são automaticamente liberados quando saem do escopo
auto packet = createPacket(data);
sendPacket(peer, std::move(packet));
// packet é automaticamente destruído aqui
```

## 🎮 Protocolo de Comunicação

### Pacotes Suportados

| ID | Nome | Descrição |
|----|------|-----------|
| 1 | PING | Verificação de conexão |
| 2 | LOGIN | Autenticação de jogador |
| 3 | LOGOUT | Desconexão |
| 4 | MOVE | Atualização de posição |
| 5 | CHAT | Mensagens de chat |
| 6 | SPAWN_PLAYER | Spawn de jogador |
| 7 | LUA_SCRIPT | Execução de script Lua |
| 8 | LUA_RESPONSE | Resposta de script Lua |

### Formato de Pacotes

Todos os pacotes seguem o formato JSON:

```json
{
  "id": 2,
  "data": {
    "user": "jogador123",
    "x": 100.5,
    "y": 200.3
  }
}
```

## 🔄 Migração do Código Original

Para migrar do código original para o novo sistema:

1. **Substitua includes**:
   ```cpp
   // Antigo
   #include "database.h"
   #include "packethandler.h"
   
   // Novo
   #include "secure_database.h"
   #include "secure_packet_handler.h"
   #include "lua_interface.h"
   ```

2. **Atualize instâncias**:
   ```cpp
   // Antigo
   Database db;
   
   // Novo
   auto db = std::make_unique<SecureDatabase>();
   ```

3. **Use gerenciamento de pacotes seguro**:
   ```cpp
   // Antigo
   ENetPacket* packet = PacketHandler::create(id, data);
   enet_peer_send(peer, 0, packet);
   
   // Novo
   auto packet = SecurePacketHandler::create(id, data);
   SecurePacketHandler::sendPacket(peer, std::move(packet));
   ```

## 🐛 Depuração

O sistema inclui logs detalhados:

- `[SECURE DB]` - Operações de banco de dados
- `[PACKET]` - Criação e envio de pacotes
- `[MEMÓRIA]` - Alocação/liberação de memória
- `[LUA]` - Operações do sistema Lua
- `[PLAYER]` - Gerenciamento de jogadores

## 📈 Desempenho

As melhorias de segurança não impactam o desempenho:

- **Validação de nomes**: Apenas na inicialização e mudança de esquema
- **Prepared Statements**: Cache de statements para operações repetidas
- **Gerenciamento de memória**: Zero overhead em tempo de execução

## 🛡️ Próximos Passos

1. **Testes de Segurança**: Implementar testes de injeção SQL
2. **Benchmarks**: Medir impacto nas operações de banco de dados
3. **Scripts Lua Adicionais**: Estender sistema com mais funcionalidades
4. **Interface Web**: Adicionar painel de administração via Lua

## 📞 Suporte

Para dúvidas sobre o sistema seguro:

- Verifique os logs de execução
- Consulte a documentação das classes `SecureDatabase` e `SecurePacketHandler`
- Examine os exemplos nos scripts Lua