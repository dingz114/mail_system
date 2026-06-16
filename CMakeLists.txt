cmake_minimum_required(VERSION 3.16)
project(MailSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(MAILSYSTEM_DEPLOY "Copy runtime DLLs after building" ON)

# --- Find Qt ---
find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Widgets Qt6::Network)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
    set(QT_LIBS Qt5::Widgets Qt5::Network)
endif()

# --- MySQL ---
# Modify MYSQL_ROOT or MYSQL_INCLUDE_DIR to match your MySQL installation.
set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH "MySQL Server root directory")
if(EXISTS "${MYSQL_ROOT}/include/mysql.h")
    set(_MYSQL_INCLUDE_DEFAULT "${MYSQL_ROOT}/include")
elseif(EXISTS "${MYSQL_ROOT}/Library/include/mysql.h")
    set(_MYSQL_INCLUDE_DEFAULT "${MYSQL_ROOT}/Library/include")
else()
    set(_MYSQL_INCLUDE_DEFAULT "${MYSQL_ROOT}/include")
endif()

if(EXISTS "${MYSQL_ROOT}/lib/libmysql.dll")
    set(_MYSQL_DLL_DEFAULT "${MYSQL_ROOT}/lib/libmysql.dll")
elseif(EXISTS "${MYSQL_ROOT}/Library/bin/libmysql.dll")
    set(_MYSQL_DLL_DEFAULT "${MYSQL_ROOT}/Library/bin/libmysql.dll")
else()
    set(_MYSQL_DLL_DEFAULT "${MYSQL_ROOT}/lib/libmysql.dll")
endif()

set(MYSQL_INCLUDE_DIR "${_MYSQL_INCLUDE_DEFAULT}" CACHE PATH "MySQL include directory")
set(MYSQL_DLL "${_MYSQL_DLL_DEFAULT}" CACHE FILEPATH "MySQL client DLL")
set(MYSQL_IMPLIB "${CMAKE_SOURCE_DIR}/lib_mingw/libmysql.dll.a" CACHE FILEPATH "MinGW import lib for MySQL")

if(NOT EXISTS "${MYSQL_INCLUDE_DIR}/mysql.h")
    message(FATAL_ERROR "mysql.h not found at ${MYSQL_INCLUDE_DIR}/mysql.h
    Please configure with -DMYSQL_ROOT=... or -DMYSQL_INCLUDE_DIR=...")
endif()

# --- OpenSSL ---
if(DEFINED ENV{CONDA_PREFIX} AND EXISTS "$ENV{CONDA_PREFIX}/Library/include/openssl/ssl.h")
    set(_OPENSSL_ROOT_DEFAULT "$ENV{CONDA_PREFIX}/Library")
elseif(EXISTS "D:/anaconda/Library/include/openssl/ssl.h")
    set(_OPENSSL_ROOT_DEFAULT "D:/anaconda/Library")
else()
    set(_OPENSSL_ROOT_DEFAULT "D:/ProgramData/miniconda3/Library")
endif()

set(OPENSSL_ROOT "${_OPENSSL_ROOT_DEFAULT}" CACHE PATH "OpenSSL root directory")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT}/include")
set(OPENSSL_SSL_DLL "${OPENSSL_ROOT}/bin/libssl-3-x64.dll" CACHE FILEPATH "libssl DLL")
set(OPENSSL_CRYPTO_DLL "${OPENSSL_ROOT}/bin/libcrypto-3-x64.dll" CACHE FILEPATH "libcrypto DLL")
set(OPENSSL_SSL_IMPLIB "${CMAKE_SOURCE_DIR}/lib_mingw/libssl.dll.a" CACHE FILEPATH "MinGW import lib for OpenSSL")
set(OPENSSL_CRYPTO_IMPLIB "${CMAKE_SOURCE_DIR}/lib_mingw/libcrypto.dll.a" CACHE FILEPATH "MinGW import lib for libcrypto")

if(NOT EXISTS "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
    message(FATAL_ERROR "OpenSSL ssl.h not found at ${OPENSSL_INCLUDE_DIR}/openssl/ssl.h
    Please edit OPENSSL_ROOT in CMakeLists.txt. You can install OpenSSL via:
      conda install openssl
    or download from https://slproweb.com/products/Win32OpenSSL.html")
endif()

# --- Collect sources ---
file(GLOB_RECURSE SOURCES_CORE    src/core/*.cpp src/core/*.h)
file(GLOB_RECURSE SOURCES_NETWORK src/network/*.cpp src/network/*.h)
file(GLOB_RECURSE SOURCES_DB      src/database/*.cpp src/database/*.h)
file(GLOB_RECURSE SOURCES_UI      src/ui/*.cpp src/ui/*.h)

# --- Executable ---
add_executable(MailSystem
    src/main.cpp
    ${SOURCES_CORE}
    ${SOURCES_NETWORK}
    ${SOURCES_DB}
    ${SOURCES_UI}
)

# Prevent conflicts between windows.h and winsock2.h
target_compile_definitions(MailSystem PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_include_directories(MailSystem PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(MailSystem PRIVATE
    ${QT_LIBS}
    ${OPENSSL_SSL_IMPLIB}
    ${OPENSSL_CRYPTO_IMPLIB}
    ${MYSQL_IMPLIB}
    ws2_32
    wsock32
)

# --- Paths for deploy ---
get_filename_component(_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(MINGW_BIN_DIR "${_COMPILER_BIN_DIR}" CACHE PATH "MinGW runtime DLL directory")

if(CMAKE_PREFIX_PATH)
    list(GET CMAKE_PREFIX_PATH 0 _QT_PREFIX_FIRST)
    set(_QT_BIN_DEFAULT "${_QT_PREFIX_FIRST}/bin")
else()
    set(_QT_BIN_DEFAULT "")
endif()
set(QT_BIN_DIR "${_QT_BIN_DEFAULT}" CACHE PATH "Qt bin directory")
find_program(QT_DEPLOY_QT windeployqt HINTS "${QT_BIN_DIR}" NO_DEFAULT_PATH)
if(NOT QT_DEPLOY_QT)
    find_program(QT_DEPLOY_QT windeployqt)
endif()

# --- Copy DLLs to output directory ---
if(MAILSYSTEM_DEPLOY)
    if(NOT QT_DEPLOY_QT)
        message(FATAL_ERROR "windeployqt not found. Configure QT_BIN_DIR or disable MAILSYSTEM_DEPLOY.")
    endif()
    foreach(_runtime_dll
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
            "${MINGW_BIN_DIR}/libstdc++-6.dll"
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
            "${MYSQL_DLL}"
            "${OPENSSL_SSL_DLL}"
            "${OPENSSL_CRYPTO_DLL}")
        if(NOT EXISTS "${_runtime_dll}")
            message(FATAL_ERROR "Runtime DLL not found: ${_runtime_dll}")
        endif()
    endforeach()

    add_custom_command(TARGET MailSystem POST_BUILD
        COMMAND "${QT_DEPLOY_QT}" --no-translations --no-system-d3d-compiler --no-opengl-sw
            $<TARGET_FILE:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll" $<TARGET_FILE_DIR:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN_DIR}/libstdc++-6.dll" $<TARGET_FILE_DIR:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MINGW_BIN_DIR}/libwinpthread-1.dll" $<TARGET_FILE_DIR:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_DLL}" $<TARGET_FILE_DIR:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_SSL_DLL}" $<TARGET_FILE_DIR:MailSystem>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENSSL_CRYPTO_DLL}" $<TARGET_FILE_DIR:MailSystem>
        COMMENT "Deploying runtime DLLs with windeployqt..."
    )
endif()

message(STATUS "Qt prefix:    ${CMAKE_PREFIX_PATH}")
message(STATUS "MySQL:        ${MYSQL_INCLUDE_DIR}")
message(STATUS "OpenSSL:      ${OPENSSL_INCLUDE_DIR}")
