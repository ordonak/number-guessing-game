all: hw4_client hw4_server 

LIBS = -lpthread
hw4_client: hw4_client.cpp 
	g++ -g -o hw4_client hw4_client.cpp $(LIBS)

hw4_server: hw4_server.cpp
	g++ -g -o hw4_server hw4_server.cpp $(LIBS)

clean: 
	rm -f *.o hw4_server
	rm -f *.o hw4_client
