cmake_minimum_required(VERSION 3.22)
project(arana-osint-sur-sur VERSION 0.1.0 LANGUAGES CXX)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencias
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(pugixml REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# nlohmann_json and CLI11 may not be available as system packages;
# fall back to FetchContent (header-only libs)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(CLI11 QUIET)
if(NOT CLI11_FOUND)
    message(STATUS "CLI11 not found via find_package, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.2
    )
    FetchContent_MakeAvailable(CLI11)
endif()

# cpp-httplib (header-only HTTP server, used by feedback_server.cpp)
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    message(STATUS "cpp-httplib not found via find_package, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.18.1
    )
    FetchContent_MakeAvailable(httplib)
endif()

find_package(PkgConfig REQUIRED)



# Target Principal: sur-sur-daemon
add_executable(sur-sur-daemon
    src/main.cpp
    src/spider.cpp
    src/evaluator.cpp
    src/config_manager.cpp
    src/feed_generator.cpp
    src/feedback_server.cpp
    src/graph_connector.cpp
    src/media_processor.cpp
)

target_include_directories(sur-sur-daemon PRIVATE include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(sur-sur-daemon PRIVATE
    CURL::libcurl
    pugixml
    Threads::Threads
    ${SQLite3_LIBRARIES}
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
    CLI11::CLI11
    httplib::httplib
    INIReader
    inih
)

include(GNUInstallDirs)
install(TARGETS sur-sur-daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

# ============================================================================
# TARGET DE PRUEBAS: test-spider (Añadido en Fase 1.6)
# ============================================================================
add_executable(test-spider 
    src/test_spider.cpp 
    src/spider.cpp 
    src/graph_connector.cpp 
    src/config_manager.cpp
    src/feed_generator.cpp
    src/evaluator.cpp
)

target_include_directories(test-spider PRIVATE 
    include 
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(test-spider PRIVATE 
    CURL::libcurl
    pugixml
    Threads::Threads
    spdlog::spdlog
    fmt::fmt
    ${SQLite3_LIBRARIES}
    INIReader
    inih
    nlohmann_json::nlohmann_json
)

message(STATUS "Configuración lista. Target 'test-spider' disponible.")
