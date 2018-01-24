# termachat
A chatroom that runs purely in the terminal. Create a chatroom over intranet and connect multiple clients!

### Features


### In development


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

Once in the client, descriptions for supported commands can be obtained by typing ```@help```


### Bugs



### Members
Jeffrey Luo<br>
Gian Tricarico<br>
Daniel Pae
