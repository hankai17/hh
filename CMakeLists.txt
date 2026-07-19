cmake_minimum_required(VERSION 3.14)
project(HH VERSION 1.0 LANGUAGES CXX)

# ==================== 编译选项 ====================
set(CMAKE_CXX_STANDARD          14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS       OFF)

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-g -O0 -Wall -Wextra -fsanitize=undefined,address)
add_link_options(-fsanitize=undefined,address)

# ==================== 目录设置 ====================
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==================== 生成 Lexer 和 Parser ====================

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc 
            ${CMAKE_CURRENT_BINARY_DIR}/lexer.hh
    COMMAND flex --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.hh 
                 -o ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc 
                 ${SRC_DIR}/lexer.l
    DEPENDS ${SRC_DIR}/lexer.l
    COMMENT "Generating lexer with Flex"
)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/parser.cc 
            ${CMAKE_CURRENT_BINARY_DIR}/parser.hh
    COMMAND bison --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.hh 
                  -o ${CMAKE_CURRENT_BINARY_DIR}/parser.cc 
                  ${SRC_DIR}/parser.y
    DEPENDS ${SRC_DIR}/parser.y
            ${SRC_DIR}/location.hh
            ${SRC_DIR}/syntax.hh
    COMMENT "Generating parser with Bison"
)

# ==================== 公共源文件（不含 main） ====================
set(COMMON_SOURCES
    ${SRC_DIR}/common.cc
    ${SRC_DIR}/lexer_helper.cc
    ${SRC_DIR}/location.cc
    ${SRC_DIR}/syntax.cc
    ${SRC_DIR}/loader.cc
    ${SRC_DIR}/option.cc
    ${SRC_DIR}/fsa.cc
    ${SRC_DIR}/fsa_anno.cc
    ${SRC_DIR}/compiler.cc
)

# ==================== 主程序 hh ====================
add_executable(hh
    ${COMMON_SOURCES}
    ${SRC_DIR}/main.cc
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
)

target_include_directories(hh PRIVATE 
    ${SRC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ==================== 测试程序 minimize_test ====================
add_executable(minimize_test
    ${COMMON_SOURCES}
    tests/minimize_test.cc
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.cc
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
)

target_include_directories(minimize_test PRIVATE 
    ${SRC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ==================== 安装 ====================
install(TARGETS hh minimize_test DESTINATION bin)
