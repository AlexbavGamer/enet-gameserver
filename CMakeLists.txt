cmake_minimum_required(VERSION 3.15)
project(GameServer LANGUAGES CXX)

# ==============================
# Compiladores
# ==============================
set(CMAKE_C_COMPILER C:/mingw64/bin/gcc.exe)
set(CMAKE_CXX_COMPILER C:/mingw64/bin/g++.exe)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================
# Diretórios
# ==============================
set(BUILD_DIR ${CMAKE_BINARY_DIR})
set(ENET_SRC  ${BUILD_DIR}/enet-src)
set(ENET_BUILD ${BUILD_DIR}/enet-build)
set(JSON_SRC  ${BUILD_DIR}/json-src)
set(SOCI_SRC  ${BUILD_DIR}/soci-src)
set(SOCI_BUILD ${BUILD_DIR}/soci-build)
set(LUA_SRC   ${BUILD_DIR}/lua-src)
set(LUA_BUILD ${BUILD_DIR}/lua-build)
set(SOL2_SRC  ${BUILD_DIR}/sol2-src)

# ==============================
# ExternalProject: ENet
# ==============================
include(ExternalProject)
ExternalProject_Add(
    enet_external
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.17
    SOURCE_DIR ${ENET_SRC}
    BINARY_DIR ${ENET_BUILD}
    CONFIGURE_COMMAND cmake -G "Ninja" ${ENET_SRC} -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND cmake --build ${ENET_BUILD}
    INSTALL_COMMAND ""
)

# ==============================
# ExternalProject: nlohmann/json
# ==============================
ExternalProject_Add(
    json_external
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    SOURCE_DIR ${JSON_SRC}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

# ==============================
# ExternalProject: SOCI (MySQL)
# ==============================
ExternalProject_Add(
    soci_external
    GIT_REPOSITORY https://github.com/SOCI/soci.git
    GIT_TAG master
    SOURCE_DIR ${SOCI_SRC}
    BINARY_DIR ${SOCI_BUILD}
    CONFIGURE_COMMAND cmake -G "Ninja"
        -DCMAKE_BUILD_TYPE=Release
        -DSOCI_TESTS=OFF
        -DSOCI_STATIC=ON
        -DSOCI_SHARED=OFF
        -DSOCI_MYSQL=ON
        -DSOCI_ODBC=OFF
        -DSOCI_SQLITE3=OFF
        -DCMAKE_CXX_FLAGS=-fno-lto
        -DCMAKE_C_FLAGS=-fno-lto
        -DMySQL_INCLUDE_DIRS=C:/mariadb-connector-c/include/mariadb
        -DMySQL_LIBRARIES=C:/mariadb-connector-c/lib/mariadb/libmariadb.a
        ${SOCI_SRC}
    BUILD_COMMAND cmake --build ${SOCI_BUILD}
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    lua_external
    GIT_REPOSITORY https://github.com/LuaDist/lua.git
    GIT_TAG master
    SOURCE_DIR ${LUA_SRC}
    BINARY_DIR ${LUA_BUILD}

    # Executa antes da configuração: renomeia luaconf.h.orig -> luaconf.h
    PATCH_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
            ${LUA_SRC}/src/luaconf.h.orig
            ${LUA_SRC}/src/luaconf.h
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Renomeado luaconf.h.orig -> luaconf.h"

    # Configuração normal
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    BUILD_COMMAND cmake --build ${LUA_BUILD}
    INSTALL_COMMAND ""
)

# ==============================
# ExternalProject: sol2
# ==============================
ExternalProject_Add(
    sol2_external
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG main
    SOURCE_DIR ${SOL2_SRC}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# ==============================
# Executável
# ==============================
# Compilar executável com suporte a Lua
add_executable(${PROJECT_NAME} ${SRC_FILES})

# ==============================
# Dependências (sem Lua por enquanto)
# ==============================
add_dependencies(${PROJECT_NAME} enet_external json_external soci_external)

# ==============================
# Includes
# ==============================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${ENET_SRC}/include
    ${JSON_SRC}/single_include
    ${SOCI_SRC}/include
    ${SOCI_BUILD}/include
    ${LUA_SRC}/src
    ${SOL2_SRC}/include
    C:/mariadb-connector-c/include/mariadb
    C:/mingw64/include
    
    # Novos diretórios de código fonte
    ${CMAKE_SOURCE_DIR}/src/config
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/database
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/utils
)

# ==============================
# Link
# ==============================
target_link_directories(${PROJECT_NAME} PRIVATE
    ${ENET_BUILD}
    ${SOCI_BUILD}/lib
    ${LUA_BUILD}
    C:/mariadb-connector-c/lib/mariadb
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    enet
    soci_mysql
    soci_core
    mariadb
    lua
    ws2_32
    winmm
)

# ==============================
# Flags - Desabilitar LTO
# ==============================
target_compile_options(${PROJECT_NAME} PRIVATE
    -std=c++20
    -Wno-cpp
    -Wno-deprecated-declarations
    -fno-lto
)

target_link_options(${PROJECT_NAME} PRIVATE
    -fno-lto
)

# ==============================
# Output
# ==============================
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ==============================
# Pós-build: Copiar arquivos necessários
# ==============================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${LUA_BUILD}/liblua.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/liblua.dll
    COMMENT "Copiando liblua.dll para o diretório de saída"
)

# Criar diretório de scripts se não existir
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
    COMMENT "Criando diretório de scripts"
)

# Copiar scripts Lua para o diretório de saída
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/scripts
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
    COMMENT "Copiando scripts Lua para o diretório de saída"
)

# Copiar DLLs do MariaDB se existirem
if(EXISTS "C:/mariadb-connector-c/lib/libmariadb.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "C:/mariadb-connector-c/lib/libmariadb.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copiando libmariadb.a para o diretório de saída"
    )
endif()

# Garantir que o luaconf.h exista no diretório de build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${LUA_SRC}/src/luaconf.h
        ${LUA_BUILD}/src/luaconf.h
    COMMENT "Copiando luaconf.h para o diretório de build"
)