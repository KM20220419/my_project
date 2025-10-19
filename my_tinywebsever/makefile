CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./my_timer/my_lst_timer.cpp ./my_http/my_http_conn.cpp ./my_log/log.cpp ./my_CGImysql/my_sql_connection_sql.cpp  my_websever.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server
