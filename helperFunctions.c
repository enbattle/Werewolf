#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers.h"

// Checks what command was requested in the buffer
int check_command(char* request, char* buffer, int request_length) {
	int i;
	for(i=0; i<request_length; i++) {
		if(request[i] == buffer[i]) {
			continue;
		}
		else {
			return 0;
		}
	}
	return 1;
}

// Swapping-the-position function for the quicksort of the user list
void sortAllUsersSwap(struct users* user1, struct users* user2)
{
	char* tempUser1 = calloc(MAX_NAME_LENGTH, sizeof(char));
	char* tempUser2 = calloc(MAX_NAME_LENGTH, sizeof(char));
	int tempFD1;
	int tempFD2;

	// Swap the userIDs
	strcpy(tempUser1, user1->userID);
	strcpy(tempUser2, user2->userID);
	strcpy(user1->userID, tempUser2);
	strcpy(user2->userID, tempUser1);

	// Swap the file descriptors
	tempFD1 = user1->fd;
	tempFD2 = user2->fd;
	user1->fd = tempFD2;
	user2->fd = tempFD1;

	free(tempUser1);
	free(tempUser2);
}

// Find the bound for the pivot and restructure the array 
int sortPartition(struct users* usersListStruct, int start, int end)
{
	char* pivotUserID = calloc(MAX_NAME_LENGTH, sizeof(char));

	// Choose pivot to be the last element of the subarray
	strcpy(pivotUserID, usersListStruct[end].userID);

	int current = start;

	int i;
	for(i=current; i<end; i++)
	{
		// If element less than pivot, swap it to the front of the array
		if(strcmp(pivotUserID, usersListStruct[i].userID) > 0){
			sortAllUsersSwap(&usersListStruct[current], &usersListStruct[i]);
			current +=1;
		}
	}

	// Swap the pivot with the current index after the last lesser element has been swapped
	sortAllUsersSwap(&usersListStruct[current], &usersListStruct[end]);

	free(pivotUserID);

	return current;
}

// Sort the users alphabetically using quicksort
void sortAllUsers(struct users* usersListStruct, int start, int end) 
{
	if(start < end)
	{
		// Find the bound that divides the subarray into lower (less than pivot) 
		// 	and upper (greater than pivot) 
		int bound = sortPartition(usersListStruct, start, end);

		// Recursing through the lower subarray
		sortAllUsers(usersListStruct, start, bound-1);

		// Recursing through the upper subarray
		sortAllUsers(usersListStruct, bound+1, end);  
	}
}