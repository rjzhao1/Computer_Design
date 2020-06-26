//Ricky Zhao
//rjzhao@ucsc.edu
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<sys/socket.h>
#include <sys/types.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<iostream>
#include<string.h>
#include<err.h>
#include<errno.h>
#include <netdb.h>
#define PORT 80

using namespace std;

const size_t BUFF_SIZE =32768;

//Parses the header word by word
string parse_header(char header[],size_t ind) {
  string word;
  while(header[ind]!=' '&&header[ind]!='\n'){
    word+=header[ind];
    ind++;
  }
  return word;
}

//check if a filename is valid
bool valid_filename(string filename){
  bool result=true;
  for(size_t i =0;i<filename.length();i++){
    if(!(filename[i]>='a' && filename[i]<='z')
       && !(filename[i]>='A' && filename[i]<='Z')
       && filename[i]!='-' && filename[i]!='_'&&
       !isdigit(filename[i])){
      result=false;
    }
  }
  return result;
}

//function to handle GET Request
void get_funct(int socket, const char *filename){
  int fd = open(filename,O_RDONLY);
  if(fd==-1){
    if(errno==EACCES){ //if we do not have permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
      send(socket,fail,strlen(fail),0);
    }else{ //if file does not exist
      char *fail=(char *)"HTTP/1.1 404 File not found\r\n";
      send(socket,fail,strlen(fail),0);
    }
  }else{
    //read in file into buffer and send content to client
    char *buff = (char*)malloc(BUFF_SIZE*sizeof(char));
    size_t length,content_len=0;
    for(;;){
      length=read(fd, buff, sizeof(buff));
      content_len+=length;
      if(length==0) break;
      send(socket,buff,length,0);
      memset(buff, 0,BUFF_SIZE*sizeof(char));
    }
    //Send success message to client
    char *success=(char *)"HTTP/1.1 200 OK\r\nContent-Length: ";
    string content_length=to_string(content_len)+"\r\n";
    send(socket,success,strlen(success),0);
    send(socket,content_length.c_str(),strlen(content_length.c_str()),0);
    free(buff);
  }
  close(fd);
}

//function to handle PUT Request
void put_funct(int socket, string filename,size_t length){
  int fd = open(filename.c_str(),O_WRONLY);
  int write_access;
  bool created=false;
  if(errno==EACCES){//if we do not have permission to the file
    char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
    send(socket,fail,strlen(fail),0);
  }else{
    if(fd==-1){
      close(fd);
      fd = open(filename.c_str(),O_CREAT|O_WRONLY);
      created=true;
    }

    //recv content_length amount of data from client
    char *buff = (char*)malloc(length*sizeof(char));
    recv(socket, buff, length,0);
    write_access=write(fd,buff,length);
    if(created){//Send 201 code if we have created a file
      char *success=(char *)"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\n";
      send(socket,success,strlen(success),0);
    }else if(write_access==-1){
      //if we do not have write permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n\n";
      send(socket,fail,strlen(fail),0);
    }else{//send success message to client
      char *success=(char *)"HTTP/1.1 200 OK\r\n\n";
      send(socket,success,strlen(success),0);
    }
    close(fd);
    free(buff);
  }
}

int main(int argc, char const *argv[]){
  int server_fd,new_socket,valread;
  struct sockaddr_in address;
  struct hostent *addread;
  int opt =1;
  int addrlen = sizeof(address);
  char buffer[4096]={0}; //4 Kib buffer for header

  if((server_fd=socket(AF_INET,SOCK_STREAM,0))==0){
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                &opt, sizeof(opt)))
  {
      perror("setsockopt");
      exit(EXIT_FAILURE);
  }

  //if there is too many arguments
  if(argc>3){
    perror("USAGE: ./httpserver [address] [port]");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; //binds to all avalible addresses
  //If there is a second argument set check if it is valid
  if(argc>1){
    addread =gethostbyname(argv[1]);
    if(addread==NULL){
      perror("Invalid Address");
      exit(EXIT_FAILURE);
    }
    //if there is a third argument set port to third argument
    if(argc>2){
      int port=atoi(argv[2]);
      address.sin_port = htons(port);
    }else{
      address.sin_port = htons( PORT );
    }
  }else{
    address.sin_port = htons( PORT );
  }

  if(bind(server_fd,(struct sockaddr *) &address,sizeof(address))<0){
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if(listen(server_fd,3) < 0){
    perror("listen");
    exit(EXIT_FAILURE);
  }
  while(true){
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                           (socklen_t*)&addrlen))<0)
      {
        perror("accept");
        exit(EXIT_FAILURE);
      }
    //receiving the header
    valread=recv(new_socket,buffer,sizeof(buffer),0);
    size_t i =0;
    string op,filename,protocol,content_header;
    size_t content_len;

    //parsing the header
    op=parse_header(buffer,i);
    i+=op.length()+2;
    filename=parse_header(buffer,i);
    i+=filename.length()+1;
    protocol=parse_header(buffer,i);
    i+=protocol.length()+1;

    //check if filename is valid
    if(filename.length()==27&&valid_filename(filename)){
      if(op.compare("GET")==0){//if Request is a GET
        get_funct(new_socket,filename.c_str());
      }else if(op.compare("PUT")==0){//if Request is a PUT
        //Parses the content length from the header
        while (content_header.compare("Content-Length:")!=0) {
          content_header=parse_header(buffer,i);
          i+=content_header.length()+1;
        }
        content_len=stoi(parse_header(buffer,i));
        put_funct(new_socket,filename,content_len);
      }else{//If if put is not GET or PUT
        char *fail=(char *)"HTTP/1.1 500 Internal Server Error\r\n\n";
        send(new_socket,fail,strlen(fail),0);
      }
    }else{//If filename is not valid
      char *fail=(char *)"HTTP/1.1 400 Bad Request\r\n\n";
      send(new_socket,fail,strlen(fail),0);
    }
    close(new_socket);
  }
  close(server_fd);
  return 0;
}
