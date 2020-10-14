#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "helpers.h"

#define INTRODUCTION 4096
#define BUFFER_SIZE 1024
#define ADDRBUFFER 512
#define MAX_NAME_LENGTH 20
#define MAX_CLIENTS 9

// Array of current players
struct users* usersListStruct;
int currentUsersNum = 0;
fd_set readfds;

// Multithread to take care of player connections
void* playerConnection(void* newConnection) {
	int listenfd = *((int*) newConnection);
	free(newConnection);

	// Check if the client has already logged in
	int login = 0;

	// Actual userid that will be remembered
	char* savedUserID = calloc(MAX_NAME_LENGTH, sizeof(char));

	// Check for activity on each of the established connections
	while(1) {

		fflush(stdout);
		FD_SET(listenfd, &readfds);
		
		if(FD_ISSET(listenfd, &readfds)) {
			int i,j;

			// Creating buffers
			char* buffer = calloc(BUFFER_SIZE, sizeof(char));
			char* userid = calloc(MAX_NAME_LENGTH, sizeof(char));

			// Read in the number of bytes
			int bytes_read = recv(listenfd, buffer, BUFFER_SIZE - 1, 0);

			// If no bytes are read, player has lost connection
			// Else, set up player for the game
			if(bytes_read == 0) {
				// Print client disconnection
				printf("CHILD <%ld>: Player disconnected\n", pthread_self());
				close(listenfd);
				
				// Remove the client from users list
				for(i=0; i<currentUsersNum; i++) {
					if(strcmp(savedUserID, usersListStruct[i].userID) == 0) {
						for(j=i; j<currentUsersNum; j++) {
							strcpy(usersListStruct[j].userID, usersListStruct[j+1].userID);
							usersListStruct[j].fd = usersListStruct[j+1].fd;
						}
						currentUsersNum -= 1;
						break;
					}
				}

				// Freeing memory
				free(savedUserID);
				free(userid);
				free(buffer);

				// Exit the thread here and close the connection
				fflush(stdout);
				pthread_exit(NULL);
			}
			else {
				buffer[bytes_read] = '\0';
				int user_found = 0;

				///////////////////////////////////////////////////////////////////////////////////
				// Check for a user's LOGIN request
				if(check_command("LOGIN", buffer, 5)) {
					if(!login) {
						int index = 0;
						int buffer_index = 0;

						// Move past the "LOGIN"
						while(buffer[buffer_index] != ' ') {
							buffer_index++;
							if(buffer[buffer_index] == ' ') {
								buffer_index++;
								break;
							}
						}

						// Copy the userid
						while(buffer[buffer_index] != '\n') {
							userid[index] = buffer[buffer_index];
							index++;
							buffer_index++;
							if(buffer[buffer_index] == '\n') {
								userid[index] = '\0';
								break;
							}
						}

						index = 0;

						// Remember the userid
						strcpy(savedUserID, userid);

						// Print out request
						printf("CHILD <%ld>: Received LOGIN request for userid %s\n", pthread_self(), savedUserID);
						
						if(strlen(savedUserID) >= 4 && strlen(savedUserID) <= 16) {
							// Check if the user already exists
							for(i=0; i<currentUsersNum; i++) {
								if(strcmp(usersListStruct[i].userID, savedUserID) == 0) {
									user_found = 1;
									break;
								}
							}

							// If the user already exists, send message that they are already
							// connected; otherwise, send message "OK"
							if(user_found == 0) {
								strcpy(usersListStruct[currentUsersNum].userID, savedUserID);
								usersListStruct[currentUsersNum].fd = listenfd;
								currentUsersNum += 1;

								// Sort the array user structs
								int start = 0;
								int end = currentUsersNum-1;
								sortAllUsers(usersListStruct, start, end);

								// Send acknowledgement message that user is already connected
								send(listenfd, "OK!\n", 4, 0);

								// User is now logged in
								login = 1;

							}
							else {
								// Print error message of already connected
								printf("CHILD <%ld>: Sent ERROR (Already connected)\n", pthread_self());

								send(listenfd, "ERROR Already connected\n", 24, 0);
							}
						}
						else {
							// Print error message of already connected
							printf("CHILD <%ld>: Sent ERROR (Invalid userid)\n", pthread_self());

							// Send acknowledgement message that the userid is invalid
							send(listenfd, "ERROR Invalid userid\n", 21, 0);
						}
					}
					else {
						// Print error message of already connected
						printf("CHILD <%ld>: Sent ERROR (Already connected)\n", pthread_self());

						send(listenfd, "ERROR Already connected\n", 24, 0);
					}
				}
				else if(check_command("WHO", buffer, 3)) {
					// Print out request
					printf("CHILD <%ld>: Rcvd WHO request\n", pthread_self());

					char* list_of_names = calloc(BUFFER_SIZE, sizeof(char));

					strcpy(list_of_names, "OK!\n");

					// Add all users to the list of names
					for(i=0; i<currentUsersNum; i++) {
						strcat(list_of_names, usersListStruct[i].userID);
						strcat(list_of_names, "\n");
					}

					// Send acknowledgement message and list out all current users
					send(listenfd, list_of_names, strlen(list_of_names), 0);
				}
				else {
					// Print error message of invalid request
					printf("CHILD <%ld>: Sent ERROR (Invalid request)\n", pthread_self());

					// Send acknowledgement message that user is already connected
					send(listenfd, "ERROR Invalid request!\n", 23, 0);
				}
			}

			free(userid);
			free(buffer);
		}
	}
}

int main(int argc, char* argv[]) {
	if(argc != 2) {
		fprintf(stderr, "ERROR: Invalid arguments - missing port!\n");
		return EXIT_FAILURE;
	}

	// disable buffered output
	setvbuf(stdout, NULL, _IONBF, 0);

	// Allocate memory for array of user structs
	usersListStruct = calloc(MAX_CLIENTS, sizeof(struct users));

	// Read in the port number;
	unsigned short port = (short) atoi(argv[1]);

	// Creating the listener socket for TCP
	int sock_TCP = socket(AF_INET, SOCK_STREAM, 0);

	if (sock_TCP < 0) {
		fprintf(stderr, "MAIN: ERROR socket creation failed!\n");
		return EXIT_FAILURE;
	}

	// Creating server and client socket structures
	struct sockaddr_in server;
	struct sockaddr_in client;

	// Lets us rerun the server immediately after we kill it
	// int optval = 1;
	// setsockopt(sock_TCP, SOL_SOCKET, SO_REUSEADDR, (const void*) &optval, sizeof(int));
	// setsockopt(sock_UDP, SOL_SOCKET, SO_REUSEADDR, (const void*) &optval, sizeof(int));

	// Build the server's internet address
	// bzero((char*) &server, sizeof(server));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	int len = sizeof(server);

	// Binding the TCP socket to the server for TCP connections
	if(bind(sock_TCP, (struct sockaddr*) &server, len) < 0) {
		fprintf(stderr, "MAIN: ERROR bind to TCP socket failed!\n");
		return EXIT_FAILURE;
	}

	// Listen for the indicated number of players
	listen(sock_TCP, 9);

	printf("MAIN: Started Werewolf hosting server!\n");
	printf("MAIN: Listening for player connections on port: %d\n", port);
	int fromlen = sizeof(client);

	// Clear the descriptor
	FD_ZERO(&readfds);

	while(1) {
		// Creating buffers
		char* buffer = calloc(BUFFER_SIZE, sizeof(char));

		// Set the sockets
		FD_SET(sock_TCP, &readfds);

		// Find the number of ready descriptors
		int ready = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

		// If there are no ready descriptors, reset the loop
		if(ready == 0) {
			continue;
		}

		// If TCP socket is ready, then accept the connection by creating a thread
		if(FD_ISSET(sock_TCP, &readfds)) {
			int connection = accept(sock_TCP, (struct sockaddr *) &client, (socklen_t *) &fromlen);

			char* addr_buffer = calloc(ADDRBUFFER, sizeof(char));
			inet_ntop(AF_INET, &client.sin_addr, addr_buffer, ADDRBUFFER);

			// Print out IP Address of client
    	printf("MAIN: Received incoming player connection from: %s\n", addr_buffer);

			// Create a child thread to host the connection
			pthread_t tid;
			int* new_fd = calloc(1, sizeof(int));
			*new_fd = connection;

			// Send a message greeting the player
			char* greeting = calloc(INTRODUCTION, sizeof(char));
			strcpy(greeting, 
				"Hello, welcome to Werewolf!\n\n"
				"I, Wolfy, will be your moderator for this game!\n\n"
				"Here is how to play the game:\n"
				"\t Roles:\n"
				"\t\t Regular Villager: No special skills\n"
				"\t\t Seer (Villager): Can ask about a specific player's status (are they the werewolf?)\n"
				"\t\t Doctor (Villager): Can heal someone about to be killed by a werewolf\n"
				"\t\t Werewolf: Survive until the end\n\n"
				"\t Game:\n"
				"\t\t The game is divided into 'Night' and 'Day'. During the 'Night', the regular villagers a\n"
				"\t\t asleep. The werewolves will be asked to wake up and will have to choose one villager to kill.\n"
				"\t\t The seer will be asked to wake up and be able to ask about about the status of a specific\n"
				"\t\t player, and the moderator will tell them who that player is (whether they are a werewolf or\n"
				"\t\t a villager). The doctor will wake up and they will have to choose be able to choose someone\n"
				"\t\t to save (hopefully that they save the player that the werewolves are trying to kill).\n"
				"\t\t If the doctor successfully saves a villager, the moderator will say 'Someone has been saved'.\n"
				"\t\t In the 'Day', everyone decides on a player to kill. Once a player is killed, or if the players\n"
				"\t\t fail to choose a player to kill, the game goes into 'Night', and the cycle continues.\n\n"
				"\t Note:\n"
				"\t\t If a player is killed, they don't reveal their role, and even if the seer and doctor are\n"
				"\t\t both dead, the moderator will continue to ask for them to wake up.");

			send(*new_fd, greeting, strlen(greeting), 0);
			free(greeting);

			pthread_create(&tid, NULL, playerConnection, new_fd);
		}

		// Freeing memory
		free(buffer);
	}

	return EXIT_SUCCESS;
}