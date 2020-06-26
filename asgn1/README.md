RICKY ZHAO
rjzhao@ucsc.edu

Building the program:
type make in the command line with the Makefile in the same directory as the program
Running the program:
type:
./httpserver [address] [port] 
inputs in the brackets are optional

This will open a socket for the server and wait until a client connects to the server and gives the server a request.

Limitations:
httpserver might only be able to handle one request per connection to a client.

