CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

# 添加UTF-8支持
CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./log/log.cpp ./CGImysql/sql_connection_pool.cpp  webserver.cpp config.cpp ./blog/blog_handler.cpp ./blog/markdown_parser.cpp ./blog/image_uploader.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient -lssl -lcrypto

clean:
	rm  -r server
