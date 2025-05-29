struct node{
    char *id;               //jobID
    char *job;              //job
    int isRunning;          //Flag for wether the job has been executed or not
    int socket;             //Store the socket for each job
    struct node *next;      //Pointer to the next node of the queue
};

typedef struct node NODE;

struct queue{
	NODE *front;          //Pointer to the front of the queue
    NODE *end;            //Pointer to the rear of the queue
};

typedef struct queue QUEUE;

void init(QUEUE *q);
int isEmpty(QUEUE *q);
void add(QUEUE *q, char *id, char *job, int socket);
void stopvol(QUEUE *q, char *id);
char* getInfo(QUEUE *q);
char* getQueuedJobs(QUEUE *q);
int getIsRunning(QUEUE *q, char *id);
void setRunning(QUEUE *q);
char* getFirstJob(QUEUE *q);
char* getFirstJobID(QUEUE *q);
void deleteNode(QUEUE *q, NODE *prev, NODE *cur); 
int getSocket(QUEUE *q, char *id);
int hasPendingJobs(QUEUE *q);
int count(QUEUE *q);
int lessThanN(QUEUE *q, int N);