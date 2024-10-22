CXX = g++

LIBS = -lssl -lcrypto -lpthread

all: client server 

client: client.cpp
	$(CXX) -o client client.cpp $(LIBS)

server: server.cpp
	$(CXX) -o server server.cpp $(LIBS)

clean:
	rm -f client server *.o