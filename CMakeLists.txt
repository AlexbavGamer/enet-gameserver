cmake_minimum_required(VERSION 3.20)
project(GameServer LANGUAGES C CXX)

# ----------------------------------------------------------------------
# Configurações gerais
# ----------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_COMPILER   "C:/mingw64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/mingw64/bin/g++.exe")

set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")

# ----------------------------------------------------------------------
# Defines
# ----------------------------------------------------------------------
add_compile_definitions(
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    _POSIX_C_SOURCE=200809L
    __USE_MINGW_ANSI_STDIO=1
    __STDC_WANT_LIB_EXT1__=1
)

# ----------------------------------------------------------------------
# Dependências via FetchContent
# ----------------------------------------------------------------------
include(FetchContent)

# --- ENet ---
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.17
)
FetchContent_MakeAvailable(enet)

# --- nlohmann/json ---
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- sol2 ---
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(sol2)

# --- Lua ---
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.6
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
add_library(lua_static STATIC ${LUA_SOURCES})
set_target_properties(lua_static PROPERTIES LINKER_LANGUAGE C)
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")

# ----------------------------------------------------------------------
# SOCI – MYSQL (MariaDB)
# ----------------------------------------------------------------------

# Configurações SOCI ANTES do FetchContent
set(SOCI_TESTS OFF CACHE BOOL "" FORCE)
set(SOCI_STATIC ON CACHE BOOL "" FORCE)
set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
set(SOCI_CORE ON CACHE BOOL "" FORCE)
set(SOCI_EMPTY ON CACHE BOOL "" FORCE)
set(SOCI_MYSQL ON CACHE BOOL "" FORCE)

# Configurar MySQL/MariaDB
set(MySQL_INCLUDE_DIRS "C:/mariadb-connector-c/include/mariadb" CACHE PATH "" FORCE)
set(MySQL_LIBRARIES "C:/mariadb-connector-c/lib/mariadb/liblibmariadb.dll.a" CACHE FILEPATH "" FORCE)
set(MySQL_FOUND TRUE CACHE BOOL "" FORCE)

FetchContent_Declare(
    soci
    GIT_REPOSITORY https://github.com/SOCI/soci.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(soci)

# ----------------------------------------------------------------------
# OpenSSL
# ----------------------------------------------------------------------
set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
find_package(OpenSSL REQUIRED)

# ----------------------------------------------------------------------
# Projeto
# ----------------------------------------------------------------------
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# ----------------------------------------------------------------------
# Includes
# ----------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${json_SOURCE_DIR}/single_include"
    "${sol2_SOURCE_DIR}/include"
    "${lua_SOURCE_DIR}"
    "${enet_SOURCE_DIR}/include"
    "${OPENSSL_INCLUDE_DIR}"
    "C:/mariadb-connector-c/include/mariadb"
    "${soci_SOURCE_DIR}/include"
)

# ----------------------------------------------------------------------
# Linkagem
# ----------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    enet
    soci_core
    soci_mysql
    soci_empty
    lua_static
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    "C:/mariadb-connector-c/lib/mariadb/liblibmariadb.dll.a"
    ws2_32 crypt32 wsock32 advapi32 gdi32 user32 kernel32
    secur32 bcrypt rpcrt4 userenv version winmm
)

# ----------------------------------------------------------------------
# Flags de compilação
# ----------------------------------------------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-cpp -Wno-deprecated-declarations -fno-lto
    -D_GLIBCXX_USE_C99 -D_GLIBCXX_HAVE_WCSTOF
)

target_link_options(${PROJECT_NAME} PRIVATE
    -fno-lto -static-libgcc -static-libstdc++
)

# ----------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ----------------------------------------------------------------------
# Pós-build - Copia scripts
# ----------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/src/scripts")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/src/scripts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts"
    )
endif()

# ----------------------------------------------------------------------
# Pós-build - Copia DLLs necessárias
# ----------------------------------------------------------------------

# MariaDB DLL
if(EXISTS "C:/mariadb-connector-c/lib/mariadb/libmariadb.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/mariadb-connector-c/lib/mariadb/libmariadb.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copiando libmariadb.dll..."
    )
endif()

# OpenSSL DLLs do MSYS2
set(OPENSSL_DLL_PATH "C:/msys64/mingw64/bin")
set(OPENSSL_DLLS
    "libssl-3-x64.dll"
    "libcrypto-3-x64.dll"
)

foreach(dll ${OPENSSL_DLLS})
    if(EXISTS "${OPENSSL_DLL_PATH}/${dll}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_DLL_PATH}/${dll}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copiando ${dll}..."
        )
    endif()
endforeach()

# MinGW Runtime DLLs (se necessário)
set(MINGW_DLL_PATH "C:/mingw64/bin")
set(MINGW_DLLS
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

foreach(dll ${MINGW_DLLS})
    if(EXISTS "${MINGW_DLL_PATH}/${dll}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_DLL_PATH}/${dll}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copiando ${dll}..."
        )
    endif()
endforeach()

# Mensagem de sucesso
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "✓ Build completo! Todas as DLLs necessárias foram copiadas."
)