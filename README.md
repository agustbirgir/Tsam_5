Group (117) - Elvar23 - Agust23
This is the submission for the Bonus Points for (A) - Early Bot

This was created for c++ version 23 and was coded on linux systems.

# What to do to run the project

1. make clean

2. make

3. Open 5 different teminals to test fully, 3 are for servers, 2 are for clients, this is done over the local network, but later will be added to the TSAM server:


# Commands

Server setup
- ./tsamgroup117 <listen_port> <group_id> [peer1_ip:port] [peer2_ip:port] ...
or just this for listen only server:
- ./tsamgroup117 <listen_port> <group_id> 

client setup
- ./client <server_ip> <server_port>

Retrieves one message from teh server
- GETMSG

Sends a message to a specific group id
- SENDMSG,<GROUP_ID>,<message contents>

Replies with the server it is connected to
- LISTSERVERS


# Setup example

Server 1:
./tsamgroup117 4044 "A5_A"

Server 2:
./tsamgroup117 4045 "A5_B" 127.0.0.1:4044

Server 3:
./tsamgroup117 4046 "A5_C" 127.0.0.1:4045

Client 1:
./client 127.0.0.1 4044

Client 2:
./client 127.0.0.1 4046


