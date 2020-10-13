#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 2048
#define ADDRBUFFER 1024
#define MAX_NAME_LENGTH 20
#define MAX_CLIENTS 8

// User strict object
//  - userID: in-game-name
//  - fd: file descriptor for client
struct users {
	char userID[MAX_NAME_LENGTH];
	int fd;
};

// Array of current players
struct users* usersListStruct;
int currentUsersNum = 0;
fd_set readfds;

// Swapping-the-position function for the quicksort of the user list
void sortAllUsersSwap(struct users* user1, struct users* user2)
{
	char* tempUserID = calloc(MAX_NAME_LENGTH, sizeof(char));
	int tempFD;

	strcpy(tempUserID, user1->userID);
	strcpy(user1->userID, user2->userID);
	strcpy(user2->userID, tempUserID);

	tempFD = user1->fd;
	user1->fd = user2->fd;
	user2->fd = tempFD;

	free(tempUserID);
}

// Sort the users alphabetically using quicksort
void sortAllUsers(int start, int end) 
{
	if(currentUsersNum <= 1) 
	{
		return;
	}

	int i = 0;

	// Pivot will always be the last element in the array and subarrays
	char pivotUserID[MAX_NAME_LENGTH]; 
	strcpy(pivotUserID, usersListStruct[end].userID);

	// Counter to break up array into left, pivot, and right
	int current = start;

	// If user IDs are less than the pivot
	for(i=start; i<end-1; i++) {
		if(strcmp(pivotUserID, usersListStruct[i].userID) > 0 || 
			strcmp(pivotUserID, usersListStruct[i].userID) > 0) {
			sortAllUsersSwap(&usersListStruct[current], &usersListStruct[i]);
			current +=1;
		}
	}

	// Sort left side of user struct recursively
	sortAllUsers(start, current-2);

	// Sort right side of user struct recursively
	sortAllUsers(current, end);
}

// Checks what command was requested in the buffer
int check_command(char* command, char* buffer, int command_length) {
	int i;
	for(i=0; i<command_length; i++) {
		if(command[i] == buffer[i]) {
			continue;
		}
		else {
			return 0;
		}
	}
	return 1;
}

void* playerConnection(void* newConnection) {
	int listenfd = *((int*) newConnection);
	free(newConnection);

	// Check if the client has already logged in
	int login = 0;

	// Actual userid that will be remembered
	char* actual_userid = calloc(MAX_NAME_LENGTH, sizeof(char));

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

			// If no bytes are read, close the fd
			if(bytes_read == 0) {
				// Print client disconnection
				printf("CHILD <%ld>: Client disconnected\n", pthread_self());
				close(listenfd);
				
				// Remove the client from users list
				for(i=0; i<currentUsersNum; i++) {
					if(strcmp(actual_userid, usersListStruct[i].userID) == 0) {
						for(j=i; j<currentUsersNum; j++) {
							strcpy(usersListStruct[j].userID, usersListStruct[j+1].userID);
							usersListStruct[j].fd = usersListStruct[j+1].fd;
						}
						currentUsersNum -= 1;
						break;
					}
				}

				// Freeing memory
				free(actual_userid);
				free(userid);
				free(buffer);

				// Exit the thread here and close the connection
				fflush(stdout);
				pthread_exit(NULL);
			}
			// If there are bytes, read the bytes
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
						strcpy(actual_userid, userid);

						// Print out request
						printf("CHILD <%ld>: Rcvd LOGIN request for userid %s\n", pthread_self(), actual_userid);
						
						if(strlen(actual_userid) >= 4 && strlen(actual_userid) <= 16) {
							// Check if the user already exists
							for(i=0; i<currentUsersNum; i++) {
								if(strcmp(usersListStruct[i].userID, actual_userid) == 0) {
									user_found = 1;
									break;
								}
							}

							// If the user already exists, send message that they are already
							// connected; otherwise, send message "OK"
							if(user_found == 0) {
								strcpy(usersListStruct[currentUsersNum].userID, actual_userid);
								usersListStruct[currentUsersNum].fd = listenfd;
								currentUsersNum += 1;

								// Sort the array user structs
								int start = 0;
								int end = currentUsersNum;
								sortAllUsers(start, end);

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

				///////////////////////////////////////////////////////////////////////////////////
				// Check for a user's WHO request
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

				///////////////////////////////////////////////////////////////////////////////////
				// Check for a user's LOGOUT request
				else if(check_command("LOGOUT", buffer, 6)) {
					// Print out request
					printf("CHILD <%ld>: Rcvd LOGOUT request\n", pthread_self());

					// Check if the user is already logged in
					if(!login) {
						// Print user not logged in error
						printf("CHILD <%ld>: Sent ERROR (Please log in first!)\n", pthread_self());

						send(listenfd, "ERROR Please log in first!\n", 27, 0); 
					}
					else {
						// Remove the client from users list
						for(i=0; i<currentUsersNum; i++) {
							if(strcmp(actual_userid, usersListStruct[i].userID) == 0) {
								for(j=i; j<currentUsersNum; j++) {
									strcpy(usersListStruct[j].userID, usersListStruct[j+1].userID);
									usersListStruct[j].fd = usersListStruct[j+1].fd;
								}
								currentUsersNum -= 1;
								break;
							}
						}

						// Send acknowledgement message that user is already connected
						send(listenfd, "OK!\n", 4, 0);

						// User is no longer logged in
						login = 0;

						free(actual_userid);
						actual_userid = calloc(MAX_NAME_LENGTH, sizeof(char));
					}
				}

				///////////////////////////////////////////////////////////////////////////////////
				// Check for a user's SEND request
				else if(check_command("SEND", buffer, 4)) {
					char* check_user_id = calloc(MAX_NAME_LENGTH, sizeof(char));
					char* msglen = calloc(BUFFER_SIZE, sizeof(char));
					char* message = calloc(BUFFER_SIZE, sizeof(char));
					int user_found = 0;
					int buffer_index = 0;
					int index = 0;

					// Check if logged in; SEND request is only available for logged in users
					if(login) { 
						// Move index past "SEND"
						while(buffer[buffer_index] != ' ') {
							buffer_index++;
							if(buffer[buffer_index] == ' ') {
								buffer_index++;
								break;
							}
						}

						// Copy the userid
						while(buffer[buffer_index] != ' ') {
							check_user_id[index] = buffer[buffer_index];
							buffer_index++;
							index++;
							if(buffer[buffer_index] == ' ') {
								check_user_id[index] = '\0';
								buffer_index++;
								break;
							}
						}

						// Print out request
						printf("CHILD <%ld>: Rcvd SEND request to userid %s\n", pthread_self(), check_user_id);

						// Reset index
						index = 0;

						// Check if user exists
						for(i=0; i<currentUsersNum; i++) {
							if(strcmp(usersListStruct[i].userID, check_user_id) == 0) {
								user_found = 1;
								break;
							}
						}

						// If the user does not exist, send message of unknown user; else, continue
						if(user_found == 0) {
							// Print error message of unknown userid
							printf("CHILD <%ld>: Sent ERROR (Unknown userid)\n", pthread_self());

							send(listenfd, "ERROR Unknown userid\n", 21, 0);
						}
						else {
							// Copy the msglen
							while(buffer[buffer_index] != '\n') {
								msglen[index] = buffer[buffer_index];
								buffer_index++;
								index++;
								if(buffer[buffer_index] == '\n') {
									msglen[index] = '\0';
									buffer_index++;
									break;
								}
							}

							// Reset index
							index = 0;

							// Check if msglen is between 1 and 990 (inclusive)
							int len = atoi(msglen);
							if(len >= 1 && len <= 990) {
								// Send acknowledgement message that user is already connected
								send(listenfd, "OK!\n", 4, 0);

								// Copy the msglen
								while(buffer[buffer_index] != '\n') {
									message[index] = buffer[buffer_index];
									buffer_index++;
									index++;
									if(buffer[buffer_index] == '\n') {
										message[index] = '\0';
										buffer_index++;
										break;
									}
								}

								// Reset index
								index = 0;

								// Obtain the sender userid
								char* current_user_id = calloc(MAX_NAME_LENGTH, sizeof(char));
								for(i=0; i<currentUsersNum; i++) {
									if(usersListStruct[i].fd == listenfd) {
										strcpy(current_user_id, usersListStruct[i].userID);
										break;
									}
								}

								// Obtain the recipient fd
								int recipient_fd;
								for(i=0; i<currentUsersNum; i++) {
									if(strcmp(check_user_id, usersListStruct[i].userID) == 0) {
										recipient_fd = usersListStruct[i].fd;
										break;
									}
								}

								// Create the string to send over to the indicated user
								char* new_message = calloc(BUFFER_SIZE, sizeof(char));
								strcpy(new_message, "FROM ");
								strcat(new_message, current_user_id);
								strcat(new_message, " ");
								strcat(new_message, msglen);
								strcat(new_message, " ");
								strcat(new_message, message);
								strcat(new_message, "\n");

								send(recipient_fd, new_message, strlen(new_message), 0);
							}
							else {
								// Print error message of invalid msglen
								printf("CHILD <%ld>: Sent ERROR (Invalid msglen)\n", pthread_self());

								send(listenfd, "ERROR Invalid msglen\n", 21, 0);
							}
						}
					}
					// If not logged in
					else {
						printf("CHILD <%ld>: Sent ERROR (Not logged in for SEND request!)\n", pthread_self());

						send(listenfd, "ERROR Not logged in for SEND request!\n", 38, 0);
					}
				}

				///////////////////////////////////////////////////////////////////////////////////
				// Check for a user's BROADCAST 
				else if(check_command("BROADCAST", buffer, 9)) {
					char* msglen = calloc(BUFFER_SIZE, sizeof(char));
					char* message = calloc(BUFFER_SIZE, sizeof(char));
					int buffer_index = 0;
					int index = 0;

					// Print out request
					printf("CHILD <%ld>: Rcvd BROADCAST request\n", pthread_self());

					// Move index past "BROADCAST"
					while(buffer[buffer_index] != ' ') {
						buffer_index++;
						if(buffer[buffer_index] == ' ') {
							buffer_index++;
							break;
						}
					}

					// Copy the msglen
					while(buffer[buffer_index] != '\n') {
						msglen[index] = buffer[buffer_index];
						index++;
						buffer_index++;
						if(buffer[buffer_index] == '\n') {
							msglen[index] = '\0';
							buffer_index++;
							break;
						}
					}

					// Reset index
					index = 0;

					// Check if msglen is between 1 and 990 (inclusive)
					int len = atoi(msglen);
					if(len >= 1 && len <= 990) {
						// Send acknowledgement message that user is already connected
						send(listenfd, "OK!\n", 4, 0);
						
						// Copy the message
						while(buffer[buffer_index] != '\n') {
							message[index] = buffer[buffer_index];
							index++;
							buffer_index++;
							if(buffer[buffer_index] == '\n') {
								message[index] = '\0';
								buffer_index++;
								break;
							}
						}

						// Reset index
						index = 0;

						// Obtain the sender userid
						char* current_user_id = calloc(MAX_NAME_LENGTH, sizeof(char));
						for(i=0; i<currentUsersNum; i++) {
							if(usersListStruct[i].fd == listenfd) {
								strcpy(current_user_id, usersListStruct[i].userID);
								break;
							}
						}

						// Create the string to send over to the indicated user
						char* new_message = calloc(BUFFER_SIZE, sizeof(char));
						strcpy(new_message, "FROM ");
						strcat(new_message, current_user_id);
						strcat(new_message, " ");
						strcat(new_message, msglen);
						strcat(new_message, " ");
						strcat(new_message, message);
						strcat(new_message, "\n");

						// Broadcast to all users except for oneself
						for(i=0; i<currentUsersNum; i++) {
							send(usersListStruct[i].fd, new_message, strlen(new_message), 0);
						}
					}
					else {
						// Print error message of invalid msglen
						printf("CHILD <%ld>: Sent ERROR (Invalid msglen)\n", pthread_self());

						send(listenfd, "ERROR Invalid msglen\n", 21, 0);
					}
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

	// Listen for the indicated number of clients
	listen(sock_TCP, 8);

	printf("MAIN: Started server\n");
	printf("MAIN: Listening for TCP connections on port: %d\n", port);
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
    	printf("MAIN: Rcvd incoming TCP connection from: %s\n", addr_buffer);

			// Create a child thread to host the connection
			pthread_t tid;
			int* new_fd = calloc(1, sizeof(int));
			*new_fd = connection;
			pthread_create(&tid, NULL, playerConnection, new_fd);
		}

		// Freeing memory
		free(buffer);
	}

	return EXIT_SUCCESS;
}