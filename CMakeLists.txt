cmake_minimum_required(VERSION 3.10)
project(TinyWebServer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# 添加UTF-8支持
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")

# 查找必要的库
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 查找MySQL客户端库
pkg_check_modules(MYSQL REQUIRED mysqlclient)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/timer)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/http)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/log)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/CGImysql)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/threadpool)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lock)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/blog)
include_directories(${MYSQL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

# 源文件
set(SOURCES
    main.cpp
    timer/lst_timer.cpp
    http/http_conn.cpp
    log/log.cpp
    CGImysql/sql_connection_pool.cpp
    webserver.cpp
    config.cpp
    blog/blog_handler.cpp
    blog/markdown_parser.cpp
    blog/image_uploader.cpp
)

# 创建可执行文件
add_executable(server ${SOURCES})

# 链接库
target_link_libraries(server 
    Threads::Threads
    ${MYSQL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 设置编译器和链接器标志
target_compile_options(server PRIVATE ${MYSQL_CFLAGS_OTHER})
target_link_directories(server PRIVATE ${MYSQL_LIBRARY_DIRS})

# GTest 和 GMock 支持 (可选)
find_package(GTest QUIET)
find_package(GMock QUIET)
if(GTest_FOUND AND GMock_FOUND)
    enable_testing()
    message(STATUS "GTest and GMock found, enabling tests")
elseif(GTest_FOUND)
    enable_testing()
    message(STATUS "GTest found, enabling tests (GMock not available)")
else()
    message(WARNING "GTest not found, skipping tests")
endif()

# 测试源文件 (不包含main.cpp)
set(TEST_SOURCES
    timer/lst_timer.cpp
    http/http_conn.cpp
    log/log.cpp
    CGImysql/sql_connection_pool.cpp
    webserver.cpp
    config.cpp
    blog/blog_handler.cpp
    blog/markdown_parser.cpp
    blog/image_uploader.cpp
)

# 创建测试可执行文件（仅在找到GTest时）
if(GTest_FOUND)
    add_executable(tests
        tests/test_http.cpp
        ${TEST_SOURCES}
    )

    # 链接测试库
    if(GMock_FOUND)
        target_link_libraries(tests
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
            Threads::Threads
            ${MYSQL_LIBRARIES}
            OpenSSL::SSL
            OpenSSL::Crypto
        )
    else()
        target_link_libraries(tests
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            ${MYSQL_LIBRARIES}
            OpenSSL::SSL
            OpenSSL::Crypto
        )
    endif()

    # 设置测试编译选项
    target_compile_options(tests PRIVATE ${MYSQL_CFLAGS_OTHER})
    target_link_directories(tests PRIVATE ${MYSQL_LIBRARY_DIRS})

    # 添加测试
    add_test(NAME unit_tests COMMAND tests)
    
    message(STATUS "Test target 'tests' created")
else()
    message(STATUS "Skipping test target creation - GTest not available")
endif()