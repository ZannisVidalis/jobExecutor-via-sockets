#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include "queue.h"

//Initialize the queue
void init(QUEUE *q)
{
    q->front = NULL;
    q->end = NULL;
}


//Return 1/0 if the queue is/isn't empty
int isEmpty(QUEUE *q)
{
    return (q->front == NULL);
}


//Add the job to the queue
void add(QUEUE *q, char *id, char *job, int socket)
{
    NODE *n;
    
    n = (NODE *)malloc(sizeof(NODE));
    if(!n) {printf("Memory allocation failed.\n");}

    n->id = strdup(id);
    n->job = strdup(job);
    n->isRunning = 0;                   //The job is not running at the start
    n->next = NULL;
    n->socket = socket;

    //If queue empty -> set front and end to the n node
    if(isEmpty(q)){
        q->front = n;
        q->end = n;
    }   
    //Else queue has already a node so -> Add the n node to the end of the queue
    else{
        q->end->next = n;
        q->end = n;
    }
}


//Removes jobs from the queue that aren't running
void stopvol(QUEUE *q, char *id) 
{
    NODE *prev = NULL;
    NODE *cur = q->front;

    //Traverse the queue to find the node with the given ID
    while(cur != NULL && strcmp(cur->id, id) != 0){
        prev = cur;
        cur = cur->next;
    }

    //If the node with the given ID is found
    if(cur != NULL){
        deleteNode(q, prev, cur);   //Delete the node
    } 
}


//Returns the <job_ID, job> for jobCommander
char* getInfo(QUEUE *q) 
{
    //Check if the queue is empty
    if(!isEmpty(q)) 
    {
        //Allocate memory
        char *info = (char *)malloc(1024 * sizeof(char));
        if(!info){ printf("Memory allocation failed.\n");}

        //Traverse the queue to find the last node
        NODE *cur = q->front;
        while(cur->next != NULL){
            cur = cur->next;
        }

        //Construct the triplet
        sprintf(info, "%s,%s", cur->id, cur->job);

        return info;
    }
}


//Returns the jobs that aren't running in the queue -> for the poll instruction
char* getQueuedJobs(QUEUE *q) 
{
    //Allocate memory
    char *info = (char *)malloc(PIPE_BUF * sizeof(char));
    if(!info){ printf("Memory allocation failed.\n");}

    //Traverse the queue to find nodes with isRunning = 0
    NODE *cur = q->front;
    while(cur != NULL) 
    {
        if(cur->isRunning == 0) 
        {
            //Allocate memory
            char *jobInfo = (char *)malloc(1024 * sizeof(char));
            if(!jobInfo){ printf("Memory allocation failed.\n");}

            //Concatenate the <job_ID, job>
            sprintf(jobInfo, "%s,%s\n", cur->id, cur->job);
            strcat(info, jobInfo);

            //free the mem
            free(jobInfo);
        }
        cur = cur->next;
    }

    return info;
}


//Returns 1/0 whether the specific job is/isn't running, Used in the stop instruction
int getIsRunning(QUEUE *q, char *id) 
{
    //Traverse the queue
    NODE *cur = q->front;
    while(cur != NULL){
        //Found the job
        if(strcmp(cur->id, id) == 0){
            return cur->isRunning;
        }
        cur = cur->next;
    }
    
    return -1;      //Didn't find the job
}


//Set the isRunning = 1 for the first node in the queue that has isRunning = 0
void setRunning(QUEUE *q) 
{
    //If the queue isn't empty
    if(!isEmpty(q)){
        //Traverse the queue
        NODE *cur = q->front;
        while(cur != NULL){
            if(cur->isRunning == 0){
                cur->isRunning = 1;
                return;         //Exit the function
            }
            cur = cur->next;
        }
    }
}


//Get the first job of the queue that hasn't the isRunning variable set to 1
char* getFirstJob(QUEUE *q) 
{
    //If the queue isn't empty
    if(!isEmpty(q)){
        //Traverse the queue
        NODE *cur = q->front;
        while(cur != NULL){
            if(cur->isRunning == 0){
                return cur->job;
            }
            cur = cur->next;
        }
    }
    
}


//Returns the job id of the first node in the queue that has the isRunning variable set to 0
char* getFirstJobID(QUEUE *q) 
{
    //If the queue isn't empty
    if(!isEmpty(q)){
        //Traverse the queue
        NODE *cur = q->front;
        while(cur != NULL){
            if(cur->isRunning == 0){
                return cur->id;
            }
            cur = cur->next;
        }
    }
    
}


//Helpfull function for deletion -> Used by the stopvol()
void deleteNode(QUEUE *q, NODE *prev, NODE *cur)
{
    //If the node to be deleted is the first node
    if(prev == NULL){
        q->front = cur->next;
    }
    else{
        prev->next = cur->next;
    }

    //If the node to be deleted is also the last node
    if(cur == q->end){
        q->end = prev;
    }

    //Free memory allocated for the node
    free(cur->id);
    free(cur->job);
    free(cur);
}


//Returns 1/0 whether the queue has/hasn't jobs that haven't been executed
int hasPendingJobs(QUEUE *q) 
{
    //Traverse the queue
    NODE *cur = q->front;
    while (cur != NULL) {
        if (cur->isRunning == 0) {
            return 1;           //1 if there is at least one job with isRunning == 0
        }
        cur = cur->next;
    }
    return 0;                   //0 if all jobs have been executed
}


//Returns the socket of a specific job_id
int getSocket(QUEUE *q, char *id) 
{
    //Traverse the queue
    NODE *cur = q->front;
    while(cur != NULL){
        //Found the job
        if(strcmp(cur->id, id) == 0){
            return cur->socket;
        }
        cur = cur->next;
    }
    
    return -1;                  //Didn't find the job
}


//Count how many jobs i have in the queue -> Used for the Producer(issueJob)/Consumer(Worker) part
int count(QUEUE *q) 
{
    int count = 0;

    //Traverse the queue and count the nodes
    NODE *cur = q->front;
    while(cur != NULL){
        count++;
        cur = cur->next;
    }

    return count;
}


//Returns 1/0 if there are less/more than N jobs
int lessThanN(QUEUE *q, int N)
{
    int count = 0;

    //Traverse the queue and count the nodes
    NODE *cur = q->front;
    while (cur != NULL) {
        count++;
        cur = cur->next;
    }

    if(count < N){
        return 1;       //1 if there are less than N jobs
    }

    return 0;           //0 if there are N or more jobs
}