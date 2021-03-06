# termachat
A chatroom that runs purely in the terminal. Create a chatroom over intranet and connect multiple clients! Semaphores to control access to subprocesses, as well as block race conditions. Linked list structure used to store information about chatrooms and their clients.

### Features
+ Create chatrooms and have multiple users join!
+ Create a terminal username
+ Communicate within only the chatroom
+ List out chatrooms and their members
+ Get historical messages in the chatroom

### In development

Not having to use commands for read and write.
List of chatrooms.

### How to compile

1. First, run the makefile: ```$ make```
2. Then, start the server: ```$ ./server```
3. Retrieve the IP address of the server machine: 

    ```$ ifconfig | grep "inet addr"```

   You'll need this to connect clients!

4. Connect clients! With a different computer on the same network, you can now start the client: 

    ```$ ./client <IP address>```

   Provide the client program the IP address of the server. If you're on the same machine running the server, you can also connect clients: 

    ```$ ./client```

   By default, the client will use the localhost address *(127.0.0.1)* if no other address is provided


### How to use

Full descriptions for supported commands can be obtained by typing ```@help```

### Bugs

Occational Crashes
Issues making a new server after closing one. Server occationaly does not close properly and the port is used up.
Issues establishing FIFO connection and/or keeping a second client connected (oddly enough, only on 
school computers through ssh)

### Members
Jeffrey Luo<br>
Gian Tricarico<br>
Daniel Pae
