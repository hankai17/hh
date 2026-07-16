cmake_minimum_required(VERSION 3.14)
project(HH VERSION 1.0 LANGUAGES CXX)

# ==================== 编译选项 ====================
set(CMAKE_CXX_STANDARD          14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS       OFF)

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-g -O0 -Wall -Wextra -fsanitize=undefined,address)
add_link_options(-fsanitize=undefined,address)

# ==================== 生成 Lexer 和 Parser ====================

# Flex
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc 
            ${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
    COMMAND flex --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.hh 
                 -o ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc 
                 ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
    COMMENT "Generating lexer with Flex"
)

# Bison
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/parser.cc 
            ${CMAKE_CURRENT_BINARY_DIR}/parser.hh
    COMMAND bison --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.hh 
                  -o ${CMAKE_CURRENT_BINARY_DIR}/parser.cc 
                  ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
            ${CMAKE_CURRENT_SOURCE_DIR}/location.hh
            ${CMAKE_CURRENT_SOURCE_DIR}/syntax.hh
    COMMENT "Generating parser with Bison"
)

# ==================== 源文件列表 ====================
set(SOURCES
    common.cc
    lexer_helper.cc
    location.cc
    main.cc
    syntax.cc
    loader.cc
    option.cc
    fsa.cc
    
    # 生成的文件（使用二进制目录下的路径）
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
)

# ==================== 可执行文件 ====================
add_executable(hh ${SOURCES})

# ==================== 头文件路径 ====================
target_include_directories(hh PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}     # 重要：让编译器找到 lexer.hh / parser.hh
)

# ==================== 安装（可选） ====================
install(TARGETS hh DESTINATION bin)
