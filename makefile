forking: client fserver

fserver: forking_server.o networking.o helper.o
	gcc -o server forking_server.o networking.o helper.o

client: client.o networking.o helper.o
	gcc -o client client.o networking.o helper.o

client.o: client.c networking.h helper.h
	gcc -c client.c

forking_server.o: forking_server.c networking.h helper.h
	gcc -c forking_server.c

networking.o: networking.c networking.h helper.h
	gcc -c networking.c

helper.o: helper.h
	gcc -c helper.c

clean:
	rm *.o



debug-server: debug-fserver
	valgrind --leak-check=yes --track-origins=yes ./$<

debug-client: debug-client
	valgrind --leak-check=yes --track-origins=yes ./$<

debug-forking: debug-client debug-fserver

debug-fserver: debug-forking_server.o debug-networking.o
	gcc -g -o debug-server debug-forking_server.o debug-networking.o

debug-client: debug-client.o debug-networking.o
	gcc -g -o debug-client debug-client.o debug-networking.o

debug-client.o: client.c networking.h
	gcc -g -c client.c

debug-forking_server.o: forking_server.c networking.h
	gcc -g -c forking_server.c

debug-networking.o: networking.c networking.h
	gcc -g -c networking.c
