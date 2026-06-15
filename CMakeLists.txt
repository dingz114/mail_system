cmake_minimum_required(VERSION 3.16)
project(MailSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- Find Qt ---
find_package(Qt5 COMPONENTS Widgets QUIET)
if(NOT Qt5_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    set(QT_LIBS Qt6::Widgets)
else()
    set(QT_LIBS Qt5::Widgets)
endif()

# --- MySQL ---
# Modify these paths to match your MySQL installation
set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH "MySQL Server root directory")
set(MYSQL_INCLUDE_DIR "${MYSQL_ROOT}/include")
set(MYSQL_LIB_DIR "${MYSQL_ROOT}/lib")
set(MYSQL_DLL "${MYSQL_LIB_DIR}/libmysql.dll" CACHE FILEPATH "MySQL client DLL")
set(MYSQL_IMPLIB "${CMAKE_SOURCE_DIR}/lib_mingw/libmysql.dll.a" CACHE FILEPATH "MinGW import lib for MySQL")

if(NOT EXISTS "${MYSQL_INCLUDE_DIR}/mysql.h")
    message(FATAL_ERROR "mysql.h not found at ${MYSQL_INCLUDE_DIR}/mysql.h
    Please edit MYSQL_ROOT in CMakeLists.txt to point to your MySQL installation.")
endif()

# --- OpenSSL ---
set(OPENSSL_ROOT "D:/ProgramData/miniconda3/Library" CACHE PATH "OpenSSL root directory")
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
set(QT_BIN_DIR "${CMAKE_PREFIX_PATH}/bin")
set(QT_DEPLOY_QT "${QT_BIN_DIR}/windeployqt.exe")
set(MINGW_BIN_DIR "D:/huawei/qt/Tools/mingw1310_64/bin")

# --- Copy DLLs to output directory ---
add_custom_command(TARGET MailSystem POST_BUILD
    # Run windeployqt to copy all Qt DLLs, plugins, etc.
    COMMAND "${QT_DEPLOY_QT}" --no-translations --no-system-d3d-compiler --no-opengl-sw
        $<TARGET_FILE:MailSystem>
    # Copy MinGW runtime DLLs (windeployqt doesn't copy these)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll" $<TARGET_FILE_DIR:MailSystem>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN_DIR}/libstdc++-6.dll" $<TARGET_FILE_DIR:MailSystem>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN_DIR}/libwinpthread-1.dll" $<TARGET_FILE_DIR:MailSystem>
    # Copy MySQL DLL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MYSQL_DLL}" $<TARGET_FILE_DIR:MailSystem>
    # Copy OpenSSL DLLs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENSSL_SSL_DLL}" $<TARGET_FILE_DIR:MailSystem>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENSSL_CRYPTO_DLL}" $<TARGET_FILE_DIR:MailSystem>
    COMMENT "Deploying runtime DLLs with windeployqt..."
)

message(STATUS "Qt prefix:    ${CMAKE_PREFIX_PATH}")
message(STATUS "MySQL:        ${MYSQL_INCLUDE_DIR}")
message(STATUS "OpenSSL:      ${OPENSSL_INCLUDE_DIR}")
