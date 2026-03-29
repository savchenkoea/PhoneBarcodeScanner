cmake_minimum_required(VERSION 3.50)
cmake_policy(SET CMP0167 NEW)
set(CMAKE_CXX_STANDARD 17)
set(TARGET PhoneBarcodeScanner)

# Получение номера сборки из Git (количество коммитов)
execute_process(
        COMMAND git rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BUILD_NUMBER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE GIT_ERROR
        RESULT_VARIABLE GIT_RESULT
)

# Отладочная информация
message(STATUS "Git command result: ${GIT_RESULT}")
message(STATUS "Git error (if any): ${GIT_ERROR}")
message(STATUS "Git build number: ${GIT_BUILD_NUMBER}")

# Если Git недоступен или это не Git-репозиторий, используем 1
if(NOT GIT_BUILD_NUMBER OR GIT_RESULT)
    set(GIT_BUILD_NUMBER 1)
    message(STATUS "Git not available or not a Git repository. Using BUILD_NUMBER = 1")
endif()

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

message(STATUS "Version: ${VERSION_MAJOR}.${VERSION_MINOR}.${GIT_BUILD_NUMBER}")
message(STATUS "Source DIR: ${CMAKE_SOURCE_DIR}")

# Создание заголовочного файла с номером сборки
configure_file(
        "${CMAKE_SOURCE_DIR}/templates/version.h.in"
        "${CMAKE_SOURCE_DIR}/include/version.h"
        @ONLY
)

message(STATUS "Version file generated: ${CMAKE_SOURCE_DIR}/include/version.h")

project(${TARGET}
        VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${GIT_BUILD_NUMBER})

# Компилируем для API Windows 7
if (WIN32)
    add_compile_options(-D_WIN32_WINNT=0x0601)

    # Определение архитектуры для Windows
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TARGET_ARCH "x64")
    else()
        set(TARGET_ARCH "x86")
    endif()

endif()

# Ищем библиотеку Boost
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.75 REQUIRED COMPONENTS system)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost library not found or version is too old (required 1.75+)")
endif()

# Ищем библиотеку OpenSSL
find_package(OpenSSL REQUIRED)

if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL library not found")
endif()

set(SOURCES
        phoneBarcodeScanner.cpp
        src/WSIOThread.cpp
        src/WSServerThread.cpp
        src/SSLCertManager.cpp
        src/Logger.cpp
        src/QRCodeRenderer.cpp
        src/SettingsDialog.cpp
        src/CommandHandler.cpp)

add_library(qr STATIC IMPORTED)
set_target_properties(qr PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/qrencode/qrencode_${TARGET_ARCH}.lib)

set_target_properties(qr PROPERTIES
        LINKER_LANGUAGE CXX)

if (WIN32)
    add_executable(${TARGET} WIN32 ${SOURCES})
endif()

target_include_directories(${TARGET} PRIVATE
        include
        src)

target_link_libraries(${TARGET} PRIVATE
        Boost::system qr OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    target_compile_definitions(${TARGET} PRIVATE
            _WINDOWS
            _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

    target_compile_options(${TARGET} PRIVATE /utf-8)

    # Установка имени для Windows
    set_target_properties(${TARGET} PROPERTIES
            OUTPUT_NAME "${TARGET}_win_${TARGET_ARCH}"
    )
endif ()