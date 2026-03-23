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
find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)
find_package(fmt REQUIRED)

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
