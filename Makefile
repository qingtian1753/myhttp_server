CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc
MYSQL_FLAGS = $(shell mariadb_config --cflags --libs 2>/dev/null || mysql_config --cflags --libs 2>/dev/null)

SRC = main.cpp $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = http_server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(MYSQL_FLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)