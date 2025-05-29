# jobExecutor-via-sockets

## Overview
This project is an implementation of a multithreaded job scheduling and execution system using sockets and threads. It consists of two main components:
It demonstrates how to manage and execute multiple user-defined jobs concurrently over a network. This project was developed as part of a System Programming course.
* jobExecutorServer: A multithreaded server that manages incoming job requests and dispatches them to worker threads
* jobCommander: A client program used to interact with the server by sending commands over the network

Jobs are submitted by the user through command-line instructions. They are executed according to the concurrency level, with server-side communication and task distribution handled through controller threads and a worker thread pool. Communication between client and server is established via TCP connections.
