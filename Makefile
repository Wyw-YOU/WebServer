CXX=g++
CXXFLAGS=-std=c++11 -pthread -Iinclude
LDFLAGS=-L/usr/lib64/mysql -lmysqlclient -lcrypt  # 添加库路径

SRC=src/main.cpp src/server.cpp
TARGET=server

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)  # 将链接库放在源文件后

clean:
	rm -f $(TARGET)