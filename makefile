CC = gcc
HEADERS = networking.h helper.h parser.h semaphore.h

SERV_OBJS = forking_server.o parser.o helper.o networking.o semaphore.o
CLIENT_OBJS = client.o helper.o networking.o parser.o
SERVER = server
CLIENT = client

forking: client fserver

fserver: $(SERV_OBJS)
	@$(CC) -o $(SERVER) $(SERV_OBJS)

client: $(OBJS) $(CLIENT_OBJS)
	@$(CC) -o $(CLIENT) $(CLIENT_OBJS)

client.o: client.c $(HEADERS)
	@$(CC) -c $<

forking_server.o: forking_server.c $(HEADERS)
	@$(CC) -c $<

networking.o: networking.c $(HEADERS)
	@$(CC) -c $<

helper.o: helper.c helper.h $(HEADERS)
	@$(CC) -c $<

semaphore.o: semaphore.c semaphore.h $(HEADERS)
	@$(CC) -c $<


parser.o: parser.c parser.h $(HEADERS)
	@$(CC) -c $<

clean:
	@rm *.o







debug-server: debug-fserver
	@valgrind --leak-check=yes --track-origins=yes ./$<

debug-client: debug-client
	@valgrind --leak-check=yes --track-origins=yes ./$<

debug-forking: debug-client debug-fserver

debug-fserver: debug-forking_server.o debug-networking.o
	@$(CC) -g -o debug-server debug-forking_server.o debug-networking.o

debug-client: debug-client.o debug-networking.o
	@$(CC) -g -o debug-client debug-client.o debug-networking.o

debug-client.o: client.c networking.h
	@$(CC) -g -c client.c

debug-forking_server.o: forking_server.c networking.h
	@$(CC) -g -c forking_server.c

debug-networking.o: networking.c networking.h
	@$(CC) -g -c networking.c
