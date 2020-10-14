#ifndef HELPERS_H
#define HELPERS_H

#define INTRODUCTION 4096
#define BUFFER_SIZE 1024
#define ADDRBUFFER 512
#define MAX_NAME_LENGTH 20
#define MAX_CLIENTS 9

// User struct object
//  - userID: in-game-name
//  - fd: file descriptor for client
struct users {
	char userID[MAX_NAME_LENGTH];
	int fd;
};

// Checks what request was requested in the buffer
int check_command(char* request, char* buffer, int request_length);

// Swapping-the-position function for the quicksort of the user list
void sortAllUsersSwap(struct users* user1, struct users* user2);

// Find the bound for the pivot and restructure the array 
int sortPartition(struct users* usersListStruct, int start, int end);

// Sort the users alphabetically using quicksort
void sortAllUsers(struct users* usersListStruct, int start, int end);


#endif