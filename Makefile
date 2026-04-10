CXX=g++
CXXFLAGS=-std=c++11 -pthread -Iinclude

SRC=src/main.cpp
TARGET=server

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)