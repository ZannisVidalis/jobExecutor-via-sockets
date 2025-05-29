# jobExecutor-via-sockets

## Overview
This project is an implementation of a multithreaded job scheduling and execution system using sockets and threads. It consists of two main components:
It demonstrates how to manage and execute multiple user-defined jobs concurrently over a network. This project was developed as part of a System Programming course.
* jobExecutorServer: A multithreaded server that manages incoming job requests and dispatches them to worker threads
* jobCommander: A client program used to interact with the server by sending commands over the network

Jobs are submitted by the user through command-line instructions. They are executed according to the concurrency level, with server-side communication and task distribution handled through controller threads and a worker thread pool. Communication between client and server is established via TCP connections.

## Compilation & Execution
To compile the project run: make
You need at least 2 terminals for this project

In Terminal 1, start the server: ./jobExecutorServer [portnum] [bufferSize] [threadPoolSize]. In the rest terminals start sending commands using the jobCommander like this: ./jobCommander [serverName] [portNum] [jobCommanderInputCommand] 

## jobExecutorServer
1)Initializes a pool of threadPoolSize worker threads. 2) Listens for incoming client connections on the given portnum. 3) For each connection, spawns a controller thread. 4) Maintains a shared queue for pending jobs. 5) Synchronization is handled using condition variables and mutexes to avoid busy-waiting. 6) Worker threads handle job execution and return output to clients via sockets.

### Worker Thread

* Wait for available jobs using pthread_cond_wait.

* When signaled and allowed by concurrency rules:

  * Fetch a job from the shared queue.

  * Fork a child process to:

    * Redirect STDOUT to pid.output using dup2.

    * Execute the job via execvp.

  * The parent:

    * Waits for the child to finish.

    * Sends job output back to the client

### Controller Thread
* Handles client requests.

* Parses and executes the client's command.

* Sends the message response to the client

* Uses a shared job queue (with mutex/condition variable synchronization).

* Maintains a concurrency limit to control how many worker threads can run simultaneously.

