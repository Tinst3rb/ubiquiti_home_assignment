CXX = g++
CXXFLAGS = -std=c++11 -lboost_system -pthread -Wall -Wextra -O2

all: server client

server:
	$(CXX) $(CXXFLAGS) -o server ./src/server.cpp

client:
	$(CXX) $(CXXFLAGS) -o client ./src/client.cpp

clean:
	rm -f server client