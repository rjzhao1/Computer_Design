RICKY ZHAO
rjzhao@ucsc.edu

Building the program:
type make in the command line with the Makefile in the same directory as the program
Running the program:
type:
./httpserver [address] [port] [-c] [-l] [logfile]
inputs in the brackets are optional.If you specify -l you have to include logfile.

This will open a socket for the server and wait until a client connects to the server and gives the server a request.

Limitations/Problems:
In some cases the server may not close the cliend socket. The file created by the PUT function will not have any permission. In order to read or write to the file, you have to add permission to file inorder to edit it through the server. This is the same for the logfile. The logfile created by the httpserver will not have any permission, so you would need to add the permissions manually or have an existing logfile with permissions for the program to work properly.

