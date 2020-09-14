# Goal
The goal of this program is to create a multi-threaded HTTP server that can handle GET and PUT requests.This program allows you to read and write files from the server directly via HTTP GET and POST requests. This program is also implements a cache to increase the speed of reading files from the server.

# Building the program:
Type make in the command line with the Makefile in the same directory as the program

# Running the program:
type:
./httpserver [address] [port] [-c] [-l] [logfile]
inputs in the brackets are optional.If you specify -l you have to include logfile.

-c specifies the program to turn the cache functionality on 
-l specifies the program to include a logfile that logs all request to the server

This will open a socket for the server and wait until a client connects to the server and gives the server a request.

