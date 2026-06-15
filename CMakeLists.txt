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

# --- Find OpenSSL ---
find_package(OpenSSL REQUIRED)

# --- Find MySQL ---
# Try to locate MySQL Connector/C
# Common Windows paths for MySQL Server 8.0
set(MYSQL_PATHS
    "C:/Program Files/MySQL/MySQL Server 8.0"
    "C:/Program Files/MySQL/MySQL Server 8.4"
    "C:/Program Files/MySQL/MySQL Server 9.0"
    "C:/Program Files/MySQL/MySQL Connector C 6.1"
)

set(MYSQL_FOUND FALSE)
foreach(MYSQL_PATH ${MYSQL_PATHS})
    if(EXISTS "${MYSQL_PATH}/include/mysql.h")
        set(MYSQL_INCLUDE_DIR "${MYSQL_PATH}/include" CACHE PATH "MySQL include directory")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MYSQL_LIB_DIR "${MYSQL_PATH}/lib" CACHE PATH "MySQL library directory")
        else()
            set(MYSQL_LIB_DIR "${MYSQL_PATH}/lib" CACHE PATH "MySQL library directory")
        endif()
        set(MYSQL_LIBRARY "${MYSQL_LIB_DIR}/libmysql.lib" CACHE FILEPATH "MySQL library")
        set(MYSQL_FOUND TRUE)
        message(STATUS "Found MySQL at: ${MYSQL_PATH}")
        break()
    endif()
endforeach()

if(NOT MYSQL_FOUND)
    message(FATAL_ERROR "MySQL not found. Please install MySQL Server 8.0+ or set MYSQL_INCLUDE_DIR and MYSQL_LIBRARY manually.")
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

target_include_directories(MailSystem PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(MailSystem PRIVATE
    ${QT_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MYSQL_LIBRARY}
    ws2_32
)

# Copy MySQL DLL to output directory (Windows)
if(WIN32)
    # Find libmysql.dll near the lib directory
    find_file(MYSQL_DLL
        NAMES libmysql.dll
        PATHS "${MYSQL_LIB_DIR}/../bin" "${MYSQL_LIB_DIR}" "${MYSQL_PATH}/bin"
        NO_DEFAULT_PATH
    )
    if(MYSQL_DLL)
        add_custom_command(TARGET MailSystem POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_DLL}"
            $<TARGET_FILE_DIR:MailSystem>
            COMMENT "Copying libmysql.dll to output directory"
        )
    endif()
endif()
