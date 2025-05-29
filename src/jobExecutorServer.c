#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "../include/queue.h"
#define BUFFER_SIZE 4096

int i=1, N=1;
int threadPoolSize;
pthread_t *workers;  //Array of pthreads
int bufferSize;
QUEUE q;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;             //Conditional variable used by the worker threads 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;          //Mutex used for protecting the queue

int active_workers = 0;                                     //Counts how many workers are running at the moment
pthread_cond_t bufferFull = PTHREAD_COND_INITIALIZER;       //Conditional variable when the buffer is Full

//Mutexes used at the controller thread
pthread_mutex_t mutex_controller = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conc_controller = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t poll_controller = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stop_controller = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t exit_controller = PTHREAD_MUTEX_INITIALIZER;



void* worker(void* arg)
{
    char buf2[1024], buf3[1024];

    while(1)
    {
        pthread_mutex_lock(&mutex);
        //If i dont have pending jobs to be executed the working thread can go to sleep 
        //OR N=0 -> exit instuction was executed
        if(!hasPendingJobs(&q) && N!=0){
            printf("SLEEPING %lu\n", pthread_self());
            pthread_cond_wait(&cond, &mutex);
            printf("\nWOKE UP %lu\n", pthread_self());
        }

        //Check if an exit instruction was executed
        if(N == 0){
            pthread_mutex_unlock(&mutex);
            printf("I DIED... %lu\n", pthread_self());
            break;                                              //Exit the loop and terminate the thread
        }
        pthread_mutex_unlock(&mutex);

        //worker performs actions only if the queue isn't empty
        if(!isEmpty(&q))
        {
            pthread_mutex_lock(&mutex);
            active_workers++;                                   //Increment the total active worker threads
            pthread_mutex_unlock(&mutex);

            printf("WORKER %lu IS PERFORMING: ", pthread_self());

            int pipefd[2];                                      //Descriptor for the pipe
            //Creation of the pipe - Used for passing the name of the pid.output from the child to its father
            if(pipe(pipefd) == -1){
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            char *args[100];
            char* command;

            char fileName[50];                                  //For storing the name of the file

            pthread_mutex_lock(&mutex);
            command = getFirstJob(&q);                          //Get the first job of the queue that isnt running, for execution
            strcpy(buf2, command);

            command = getFirstJobID(&q);                        //Get the first job id of the queue that isnt running, for execution
            strcpy(buf3, command);
            printf("<%s, %s>\n", buf2, buf3);

            setRunning(&q);                                     //Set the job's isRunning to 1
            pthread_mutex_unlock(&mutex);

            int tag = getSocket(&q, buf3);                      //Get the socket of that specific job_id
            
            pid_t pid = fork();

            //Fork failed
            if(pid < 0) 
            { 
                perror("Fork failed");
                exit(EXIT_FAILURE);
            }
            //Child process
            else if(pid == 0) 
            {
                sprintf(fileName, "%d.output", getpid());
                int outputFile = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (outputFile == -1) {
                    perror("File open failed");
                    exit(EXIT_FAILURE);
                }

                close(pipefd[0]);                               //Close the read end of the pipe
                //Write the output file name to the pipe
                if(write(pipefd[1], fileName, sizeof(fileName)) == -1){
                    perror("write");
                    exit(EXIT_FAILURE);
                }
                dup2(outputFile, STDOUT_FILENO);                //Redirect the output into the file
                close(outputFile);

                //Tokenize each argument
                command = strtok(buf2, " ");
                int count=0;
                while(command != NULL){
                    args[count++] = command;
                    command = strtok(NULL, " ");
                }

                execvp(args[0], args);                          //Execute the command
                
                //If execvp() fails
                perror("Execution failed");
                exit(EXIT_FAILURE);
            }
            // Parent process
            else 
            {
                waitpid(pid, NULL, 0);                          //Wait for child to finish

                close(pipefd[1]);                               //Close the write end of the pipe
                //Read the output file name from the pipe
                if(read(pipefd[0], fileName, sizeof(fileName)) == -1){
                    perror("read");
                    exit(EXIT_FAILURE);
                }

                stopvol(&q, buf3);                              //The job finished execution, so delete it from the queue

                pthread_mutex_lock(&mutex);
                //If we have any empty space, then signal the blocked issueJob's
                if(count(&q) < bufferSize){
                    pthread_cond_signal(&bufferFull);
                }
                pthread_mutex_unlock(&mutex);

                //Open the output file
                int outputFile = open(fileName, O_RDONLY);
                if (outputFile == -1) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                char buffer[BUFFER_SIZE];
                //Set up the header of the message
                snprintf(buffer, sizeof(buffer), "----- %s output start -----\n", buf3);
                send(tag, buffer, strlen(buffer), 0);

                //Send the actual output of the message
                ssize_t bytesRead;
                while((bytesRead = read(outputFile, buffer, sizeof(buffer)))>0){
                    send(tag, buffer, bytesRead, 0);
                }
                close(outputFile);                              //Close the file descriptor

                //Set up the footer of the message
                snprintf(buffer, sizeof(buffer), "----- %s output end -----\n", buf3);
                send(tag, buffer, strlen(buffer), 0);
                close(tag);

                remove(fileName);                               //Remove the temporary pid.output

                pthread_mutex_lock(&mutex);
                active_workers--;                               //Decrement the total active worker threads
                pthread_mutex_unlock(&mutex);

                printf("\nI FINISHED %lu\n", pthread_self());
            }
        }

    }
}

void* controller(void *arg)
{
    int clientSocket = *(int *)arg;
    free(arg);                          //Free the memory allocated for clientSocket in the main threads
    
    char buf[1024], com[30], tmp[1024], tmp2[1024], tmp3[1024];
    char *token, *a;

    pthread_mutex_lock(&mutex_controller);

    read(clientSocket, buf, sizeof(buf));

    token = strtok(buf, " ");           //Extract the command
    strcpy(com, token);                 //and store it in com array

    pthread_mutex_unlock(&mutex_controller);

    if(strcmp(com, "setConcurrency")==0)
    {
        pthread_mutex_lock(&conc_controller);

        int Nold = N;
        token = strtok(NULL, " ");      //Extract the value
        N = atoi(token);                //and store it

        if(N <= threadPoolSize && N <= bufferSize)
        {
            //Non valid concurrency -> Set it to 1
            if(N<=0){
                N = 1;
                snprintf(tmp, sizeof(tmp), "CONCURRENCY SET AT %d\n", N);
                write(clientSocket, tmp, strlen(tmp));
            }

            //Case where you gave bigger Concurrency than the previous one
            if(Nold < N)
            {
                pthread_mutex_lock(&mutex);
                //If the queue isnt empty - And i actually have at least N jobs to work on
                if(!isEmpty(&q) && !lessThanN(&q, N))
                {
                    int threadsToSignal = N - active_workers;
                    for(int j=0; j<threadsToSignal; j++){
                        pthread_cond_signal(&cond);         //Signal threads in order to reach N active workers
                    }

                    snprintf(tmp, sizeof(tmp), "CONCURRENCY SET AT %d\n", N);
                    write(clientSocket, tmp, strlen(tmp));
                }
                else{
                    snprintf(tmp, sizeof(tmp), "CONCURRENCY SET AT %d\n", N);
                    write(clientSocket, tmp, strlen(tmp));
                }
                pthread_mutex_unlock(&mutex);

            }
            //Case where you gave smaller Concurrency than the previous one
            else if(Nold > N)
            {
                //Ιf the main queue isnt empty then keep the previus N
                if(!isEmpty(&q)){
                    N = Nold;
                }
                snprintf(tmp, sizeof(tmp), "CONCURRENCY SET AT %d, KEPT THE PREVIOUS ONE\n", N);
                write(clientSocket, tmp, strlen(tmp));
            }
        }
        else {
            strcpy(tmp, "UNABLE TO CHANGE THE CONCURRENCY, KEPT THE PREVIOUS ONE.");
            N = Nold;
            write(clientSocket, tmp, strlen(tmp));
        }

        close(clientSocket);

        pthread_mutex_unlock(&conc_controller);
    }
    else if(strcmp(com, "issueJob")==0)
    {
        pthread_mutex_lock(&mutex);

        //The thread should wait till we have space in the buffer
        if(count(&q)>=bufferSize){
            token = strtok(NULL, "");       //Extract the unix instruction
            pthread_cond_wait(&bufferFull, &mutex);
            printf("\nI got signaled...\n");

            //Check if an exit instruction was executed
            if(N==0){
                strcpy(tmp, "SERVER TERMINATED BEFORE EXECUTION\n");
                write(clientSocket, tmp, strlen(tmp));
                close(clientSocket);
                printf("And died\n");
            }
        }
        //We have space in the buffer -> continue with the issueJob
        else{
            token = strtok(NULL, "");       //Extract the unix instruction
        }

        strcpy(tmp, token);                 //Store it in a temp string

        sprintf(tmp2, "job_%d", i);         //Concatenate "job_" with the integer i to create job ID
        i++;
    
        add(&q, tmp2, tmp, clientSocket);   //Add the command to the queue

        if(active_workers < N){
            pthread_cond_signal(&cond);     //Signal the next worker thread to enter if concurrency allows it
        }

        pthread_mutex_unlock(&mutex);

        a = getInfo(&q);                    //Get the triplet <jobID,job,queuePosition>
        snprintf(tmp, sizeof(tmp), "JOB <%s> SUBMITTED\n", a);
        write(clientSocket, tmp, strlen(tmp));
    }
    else if(strcmp(com, "stop")==0)
    {
        pthread_mutex_lock(&stop_controller);

        int tag;
        token = strtok(NULL, "");       //Extract the job_ID
        strcpy(tmp, token);             //Store it

        //SETTING UP THE MESSAGE FOR THE COMMANDER
        if(getIsRunning(&q, tmp)==0){
            snprintf(tmp2, sizeof(tmp2), "JOB <%s> REMOVED\n", token);      //Prepare the corresponding message
            write(clientSocket, tmp2, strlen(tmp2)); 

            tag = getSocket(&q, tmp);
            stopvol(&q, tmp);
            close(tag);                                                     //For removing non executing jobs
        }
        else if(getIsRunning(&q, tmp)==-1){
            snprintf(tmp2, sizeof(tmp2), "JOB <%s> NOTFOUND\n", token);     //Prepare the corresponding message
            write(clientSocket, tmp2, strlen(tmp2));         
        }
        else{
            snprintf(tmp2, sizeof(tmp2), "JOB <%s> IS RUNNING, UNABLE TO STOP IT\n", token);     //Prepare the corresponding message
            write(clientSocket, tmp2, strlen(tmp2));       
        }
        
        close(clientSocket);

        pthread_mutex_unlock(&stop_controller);
    }
    else if(strcmp(com, "poll")==0)
    {
        pthread_mutex_lock(&poll_controller);

        a = getQueuedJobs(&q);              //Get all the jobs that are not running
        strcpy(tmp, a);
        write(clientSocket, tmp, strlen(tmp));
        close(clientSocket);

        pthread_mutex_unlock(&poll_controller);
    }
    else if(strcmp(com, "exit")==0)
    {
        pthread_mutex_lock(&exit_controller);

        N = 0;

        pthread_mutex_lock(&mutex);
        //Remove all the waiting jobs
        while(hasPendingJobs(&q))
        {
            char* b = getFirstJobID(&q);
            strcpy(tmp3, b);
            int soc = getSocket(&q, tmp3);
            strcpy(tmp, "SERVER TERMINATED BEFORE EXECUTION\n");
            write(soc, tmp, strlen(tmp));
            close(soc);
            stopvol(&q, tmp3);
        }
        pthread_mutex_unlock(&mutex);

        //Signal all worker threads to wake up and exit
        pthread_cond_broadcast(&cond);
        //Signal all the blocked controller threads at the issueJob part, to wake up and exit
        pthread_cond_broadcast(&bufferFull);

        //Wait for running jobs to complete
        for(int k=0; k<threadPoolSize; k++) {
            pthread_join(workers[k], NULL);
        }

        strcpy(tmp, "SERVER TERMINATED");
        write(clientSocket, tmp, strlen(tmp));

        close(clientSocket);                //Close the socket
        
        exit(0);

        pthread_mutex_unlock(&exit_controller);
    } 
}



int main(int argc, char* argv[])
{
    //Checking the user's input
    if(argc < 4){
        fprintf(stderr, "Usage: %s [portNum] [bufferSize] [threadPoolSize]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int portNum = atoi(argv[1]);                        //Get the port number from the user
    bufferSize = atoi(argv[2]);                         //Get the buffer Size from the user
    threadPoolSize = atoi(argv[3]);                     //Get how many worker threads, the threadPool will have

    init(&q);                                           //Initialize the queue

    /*========= Thread creation =========*/
    workers = malloc(threadPoolSize * sizeof(pthread_t));

	for(int t=0; t<threadPoolSize; t++){	
		if(pthread_create(&workers[t],NULL,worker,NULL) != 0){
			perror("pthread_create failed\n");
			exit(EXIT_FAILURE);
		}
	}
    

    /*============= Setting up the sockets for connection =============*/
    int server_fd, new_socket;

    struct sockaddr_in address;
    address.sin_family = AF_INET;                       //Internet domain
    address.sin_addr.s_addr = htonl(INADDR_ANY);        //Accept connections from any IP address
    address.sin_port = htons(portNum);                  //The given port from user
    struct sockaddr * addressptr =( struct sockaddr *) & address ;
    socklen_t addrlen = sizeof(address);
    
    //Creating socket file descriptor
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
 
    //Setting socket options to allow reuse of address and port
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("setsockopt failed\n");
        exit(EXIT_FAILURE);
    }
    
    //Bind socket to address
    if(bind(server_fd, addressptr, sizeof(address)) < 0){
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }

    //Listen for connections
    if(listen(server_fd, 20) < 0){
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }
    /*=================================================================*/

   
    while(1)
    {
        //Accept connection
        if((new_socket = accept(server_fd, addressptr, &addrlen))< 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("\nNew Connection (SOCKET: %d)...\n", new_socket);

        pthread_t controllerThread;
        int *clientSocket = malloc(sizeof(int));
        *clientSocket = new_socket;
        if(pthread_create(&controllerThread, NULL, controller, clientSocket) != 0){
            perror("Controller thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    close(server_fd);

    free(workers);  //Free the threadPool array
    
    return 0;
}