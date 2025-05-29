# jobExecutor-via-sockets

## Overview
This project is an implementation of a multithreaded job scheduling and execution system using sockets and threads. It consists of two main components:
It demonstrates how to manage and execute multiple user-defined jobs concurrently over a network. This project was developed as part of a System Programming course.
* jobExecutorServer: A multithreaded server that manages incoming job requests and dispatches them to worker threads
* jobCommander: A client program used to interact with the server by sending commands

Jobs are submitted by the user via commands and executed according to the concurrency level, using named pipes and signals for communication.
