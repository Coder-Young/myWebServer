CC = g++
CFLAG = -lpthread -g

myWebServer: my_http_conn.cpp myWebServer.cpp
	$(CC) -o myWebServer my_http_conn.cpp myWebServer.cpp $(CFLAG) 
.PHONY: clean
clean:
	rm -f *.o myWebServer