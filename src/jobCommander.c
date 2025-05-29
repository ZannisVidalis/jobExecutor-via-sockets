#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <netdb.h>

int main(int argc, char* argv[])
{
    int res;

    char* instruction = argv[3];
    int portNum = atoi(argv[2]);
    char* serverName = argv[1];

    char buf[1024];                         //For sending the message to the server
    char buffer[1024] = { 0 };              //For printing the answer from the server

    //Checking the user's input
    if(argc <= 3){
        fprintf(stderr, "Usage: %s [serverName] [portNum] [jobCommanderInputCommand]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else{
        //Checking if the command is valid
        if(strcmp(instruction, "setConcurrency")==0 || strcmp(instruction, "issueJob")==0 || strcmp(instruction, "stop")==0 || strcmp(instruction, "poll")==0 || strcmp(instruction, "exit")==0)
        {
            //The command is valid
        }
        else{
            fprintf(stderr, "You have to use: \n\t1) %s issueJob <executable>\n\t2) %s setConcurrency <Number>\n\t3) %s stop <job ID>\n\t4) %s poll\n\t5) %s exit\n", argv[0], argv[0], argv[0], argv[0], argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    //Set up the message that will be sent to the server for each command
    if(strcmp(instruction, "setConcurrency")==0)
    {
        snprintf(buf, sizeof(buf), "%s %s", argv[3], argv[4]);
    }
    else if(strcmp(instruction, "issueJob")==0)
    {
        for(int i=3; i<argc-1; i++)
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s ", argv[i]);
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", argv[argc-1]);
    }
    else if(strcmp(instruction, "stop")==0)
    {
        snprintf(buf, sizeof(buf), "%s %s", argv[3], argv[4]);
    }
    else if(strcmp(instruction, "poll")==0)
    {
        strcpy(buf, argv[3]);
    }
    else if(strcmp(instruction, "exit")==0)
    {
        strcpy(buf, argv[3]);
    }


    /*============= Setting up the sockets for connection =============*/
    int client_fd;
    struct sockaddr_in serv_addr;
    struct sockaddr * serv_addrptr = ( struct sockaddr *) & serv_addr ;
    struct hostent* host;

    //Create socket
    if((client_fd = socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;             //Internet domain
    serv_addr.sin_port = htons(portNum);        //Server port from user

    //Resolve the server name to an IP address
    host = gethostbyname(serverName);
    if(host == NULL){
        perror("Cannot resolve hostname\n");
        exit(EXIT_FAILURE);
    }

    //Convert the first IP address to a string
    char* resolved_ip = inet_ntoa(*(struct in_addr*) host->h_addr_list[0]);

    //Convert the IPv4 addresses from text to binary form
    if(inet_pton(AF_INET, resolved_ip, &serv_addr.sin_addr)<= 0){
        perror("Invalid address\n");
        exit(EXIT_FAILURE);
    }

    //Connect to the server
    if(connect(client_fd, serv_addrptr, sizeof(serv_addr))< 0){
        perror("Connection Failed\n");
        exit(EXIT_FAILURE);
    }
    /*=================================================================*/

    //Send the command to the server
    send(client_fd, buf, strlen(buf), 0);
    
    //Print the message from the server
    //Special case for issueJob -> should wait for the jobs output after execution
    if(strcmp(instruction, "issueJob")==0){
        //Receive the response from the server
        while((res = read(client_fd, buffer, sizeof(buffer)))>0){
            printf("%s", buffer);
            memset(buffer, 0, sizeof(buffer)); //Clear buffer for next read
        }
    }
    //Rest instructions dont need to wait for 2nd message
    else{
        read(client_fd, buffer, sizeof(buffer));
	    printf("%s\n", buffer);
    }

    close(client_fd);               //Closing the connected socket
    
    return 0;
}