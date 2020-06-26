RICKY ZHAO
rjzhao@ucsc.edu

Building the program:
type make in the command line with the Makefile in the same directory as the program
Running the program:
type:
./httpserver [address] [port] [-N] [Number of threads] [-l] [logfile]
inputs in the brackets are optional. If you specify -N you have to include Number of threads.
If you specify -l you have to include logfile.

This will open a socket for the server and wait until a client connects to the server and gives the server a request.
Limitations/Problems:
In some cases the server may not close the cliend socket. While logging a PUT function, my program will crash and core dump. Depending on the system, you might only be able to use a txt file for your logfile.

