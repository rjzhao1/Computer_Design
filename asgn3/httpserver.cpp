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
bool log_write=false,cache_on=false;
char* logfile;
int offset =0;
string cache[4];
string cache_filename[4];
int dirty_bit[4];
int cache_size = 0;
int cache_index=0;


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

bool in_cache(string filename){
  bool result = false;
  for(int i=0;i<cache_size;i++){
    if(filename.compare(cache_filename[i])==0){
      result = true;
      cache_index = i;
    }
  }
  return result;
}

//function to handle GET Request
void get_funct(int socket, const char *filename){
  int fd = open(filename,O_RDONLY);
  int l_fd;
  char *end=(char *)"\r\n";
  char *end_log= (char *)"========\n";

  if(fd==-1){
    if(errno==EACCES){ //if we do not have permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
      send(socket,fail,strlen(fail),0);

      //logs the fail header
      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: GET ");
        char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

        pwrite(l_fd,header1,strlen(header1),offset);
        offset += strlen(header1);
        pwrite(l_fd,filename,strlen(filename),offset);
        offset+=strlen(filename);
        pwrite(l_fd,header2,strlen(header2),offset);
        offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),offset);
        offset+=strlen(end_log);
      }

    }else{ //if file does not exist
      char *fail=(char *)"HTTP/1.1 404 File not found\r\n";
      send(socket,fail,strlen(fail),0);

      //logs the fail header
      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: GET ");
        char* header2 = (char *)(" HTTP/1.1 --- response 404\n");

        pwrite(l_fd,header1,strlen(header1),offset);
        offset += strlen(header1);
        pwrite(l_fd,filename,strlen(filename),offset);
        offset+=strlen(filename);
        pwrite(l_fd,header2,strlen(header2),offset);
        offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),offset);
        offset+=strlen(end_log);
      }
    }
  }else{
    size_t content_len=0,length;
    char *success=(char *)"HTTP/1.1 200 OK\r\nContent-Length: ";
    string content_length,file_con="",name = filename;
    bool was_in_cache = in_cache(name);

    //check whether the file is in cache
    if(was_in_cache&&cache_on){
      //If the file is in cache send from cache instead
      send(socket,end,strlen(end),0);
      content_length=to_string(strlen(cache[cache_index].c_str()));
      send(socket,end,strlen(end),0);

      // Send success message to client
      send(socket,success,strlen(success),0);
      send(socket,content_length.c_str(),strlen(content_length.c_str()),0);
      send(socket,end,strlen(end),0);
      send(socket,cache[cache_index].c_str(),strlen(cache[cache_index].c_str()),0);
      send(socket,end,strlen(end),0);
    }else{
      //if file is not in cache read file from disk
      char *buff = (char*)malloc(BUFF_SIZE*sizeof(char));
      for(;;){
        length=read(fd, buff, sizeof(buff));
        content_len+=length;
        if(length==0) break;
        memset(buff, 0,BUFF_SIZE);
      }

      content_length=to_string(content_len);
      send(socket,end,strlen(end),0);

      //Send success message to client
      send(socket,success,strlen(success),0);
      send(socket,content_length.c_str(),strlen(content_length.c_str()),0);
      send(socket,end,strlen(end),0);
      close(fd);

      fd = open(filename,O_RDONLY);
      //read in file into buffer and send content to client
      for(;;){
        length=read(fd, buff, BUFF_SIZE*sizeof(char));
        content_len+=length;
        if(length==0) break;
        send(socket,buff,length,0);

        //if cache function is on make a copy of
        //the content of the file
        if(cache_on){
          file_con+=buff;
        }

        memset(buff, 0,BUFF_SIZE);
      }

      //Check if cache is full
      if(cache_on){
        //if cache is not full add the file content to
        if(cache_size<4){
          cache[cache_size]=file_con;
          cache_filename[cache_size]=name;
          cache_size++;
        }else{//if cache is full replace the first item in the cache

            //if the first item is dirty write the first item to disk
            if(dirty_bit[0]==1){
              int c_fd = open(cache_filename[0].c_str(),O_WRONLY);
              write(c_fd,cache[0].c_str(),strlen(cache[0].c_str()));
            }
            cache[0]=file_con;
            cache_filename[0]=filename;
            dirty_bit[0]=0;
          }
      }
      send(socket,end,strlen(end),0);
    }

    //logging the header to logfile
    if(log_write){
      char* header1 = (char *)("GET ");
      char* header2 = (char *)(" length ");
      char* in_c = (char *)("[was in cache]");
      char* not_in_c = (char *)("[was not in cache]");

      // print function and filename
      l_fd = open(logfile,O_CREAT|O_WRONLY);
      pwrite(l_fd,header1,strlen(header1),offset);
      offset += strlen(header1);
      pwrite(l_fd,filename,strlen(filename),offset);
      offset+=strlen(filename);
      pwrite(l_fd,header2,strlen(header2),offset);
      offset += strlen(header2);
      pwrite(l_fd,content_length.c_str(),strlen(content_length.c_str()),offset);
      offset += strlen(content_length.c_str());

      //logs whether or not the file is in cache
      if(cache_on){
        if(was_in_cache){
          pwrite(l_fd,in_c,strlen(in_c),offset);
          offset+=strlen(in_c);
          pwrite(l_fd,end,strlen(end),offset);
          offset+=strlen(end);
        }else{
          pwrite(l_fd,not_in_c,strlen(not_in_c),offset);
          offset+=strlen(not_in_c);
          pwrite(l_fd,end,strlen(end),offset);
          offset+=strlen(end);
        }
      }else{
        pwrite(l_fd,end,strlen(end),offset);
        offset+=strlen(end);
      }

      pwrite(l_fd,end_log,strlen(end_log),offset);
      offset+=strlen(end_log);
    }
  }
  close(fd);
}

//function to handle PUT Request
void put_funct(int socket, string filename,size_t length){
  int fd = open(filename.c_str(),O_RDWR);
  int l_fd;
  bool created=false;

  char *next_line=(char *)"\n";
  char *end_log= (char *)"========\n";
  char *space= (char *)(" ");

  if(errno==EACCES){//if we do not have permission to the file
    char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
    send(socket,fail,strlen(fail),0);

    if(log_write){
      l_fd = open(logfile,O_CREAT|O_RDWR);
      char* header1 = (char *)("FAIL: PUT ");
      char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

      pwrite(l_fd,header1,strlen(header1),offset);
      offset += strlen(header1);
      pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),offset);
      offset+=strlen(filename.c_str());
      pwrite(l_fd,header2,strlen(header2),offset);
      offset += strlen(header2);
      pwrite(l_fd,end_log,strlen(end_log),offset);
      offset+=strlen(end_log);
      close(l_fd);
    }

  }else{
    if(fd==-1){
      close(fd);
      fd = open(filename.c_str(),O_CREAT|O_RDWR);
      created=true;
    }
    if(created){//Send 201 code if we have created a file
      char *success=(char *)"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n";
      send(socket,success,strlen(success),0);

    }else if(errno==EACCES){
      //if we do not have write permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
      send(socket,fail,strlen(fail),0);

      //log fail header
      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: PUT ");
        char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

        pwrite(l_fd,header1,strlen(header1),offset);
        offset += strlen(header1);
        pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),offset);
        offset+=strlen(filename.c_str());
        pwrite(l_fd,header2,strlen(header2),offset);
        offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),offset);
        offset+=strlen(end_log);
        close(l_fd);
      }

    }else{//send success message to client
      char *success=(char *)"HTTP/1.1 200 OK\r\n";
      send(socket,success,strlen(success),0);
    }

    //recv content_length amount of data from client
    char *buff = (char*)malloc(length*sizeof(char));
    recv(socket, buff, length,0);
    bool was_in_cache=false;


    if(cache_on){
      was_in_cache = in_cache(filename);
      //if file is not in cache and not full, add the file content
      //to end of buffer
      if(cache_size<4&&!was_in_cache){
        cache[cache_size]=buff;
        cache_filename[cache_size]=filename;
        dirty_bit[cache_size]=1;
        cache_size++;
      }else{
        //if cache is full and file is in cache, store the file content
        //in the cache
        if(was_in_cache){
          cache[cache_index]=buff;
          dirty_bit[cache_index]=1;
        }else{//if file is not in cache
          //replace the first item in cache with the file

          //if the file is dirty, write to disk
          if(dirty_bit[0]==1){
            int c_fd = open(cache_filename[0].c_str(),O_RDWR);
            write(c_fd,cache[0].c_str(),strlen(cache[0].c_str()));
            close(c_fd);
          }
          cache[0]=buff;
          cache_filename[0]=filename;
          dirty_bit[0]=1;
        }
      }
    }else{
      write(fd,buff,length);
    }

    if(log_write&&fd!=-1){
      string con_len = to_string(length);
      char* header1 = (char *)("PUT ");
      char* header2 = (char *)(" length ");
      char* end= (char *)("\r\n");
      char* in_c = (char *)("[was in cache]");
      char* not_in_c = (char *)("[was not in cache]");
      char bytecount[9],hex[2];
      size_t byte =0;
      sprintf(bytecount,"%08zu",byte);

      // logs the PUT header
      l_fd = open(logfile,O_CREAT|O_RDWR);
      pwrite(l_fd,header1,strlen(header1),offset);
      offset += strlen(header1);
      pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),offset);
      offset+=strlen(filename.c_str());
      pwrite(l_fd,header2,strlen(header2),offset);
      offset += strlen(header2);
      pwrite(l_fd,con_len.c_str(),strlen(con_len.c_str()),offset);
      offset += strlen(con_len.c_str());

      //logs whether or not the file is in cache
      if(cache_on){
        if(was_in_cache){
          pwrite(l_fd,in_c,strlen(in_c),offset);
          offset+=strlen(in_c);
          pwrite(l_fd,end,strlen(end),offset);
          offset+=strlen(end);
        }else{
          pwrite(l_fd,not_in_c,strlen(not_in_c),offset);
          offset+=strlen(not_in_c);
          pwrite(l_fd,end,strlen(end),offset);
          offset+=strlen(end);
        }
      }else{
        pwrite(l_fd,end,strlen(end),offset);
        offset+=strlen(end);
      }

      pwrite(l_fd,next_line,strlen(next_line),offset);
      offset+=strlen(next_line);

      // logs the content of the file
      for(int i =0; i<(int)length; i++){
        if(i%20==0){
           if(i!=0){
              pwrite(l_fd,next_line,strlen(next_line),offset);
              offset+=strlen(next_line);
           }

          pwrite(l_fd,bytecount,strlen(bytecount),offset);
          offset+=strlen(bytecount);
          pwrite(l_fd,space,strlen(space),offset);
          offset+=strlen(space);
          byte+= 20;
          memset(bytecount, 0, sizeof(bytecount));
          sprintf(bytecount,"%08zu",byte);
        }
        sprintf(hex,"%02X ",buff[i]);
        pwrite(l_fd,hex,strlen(hex),offset);
        offset+=strlen(hex);
        memset(hex, 0, sizeof(hex));
      }
      pwrite(l_fd,next_line,strlen(next_line),offset);
      offset+=strlen(next_line);
      pwrite(l_fd,end_log,strlen(end_log),offset);
      offset+=strlen(end_log);
      close(l_fd);
    }
    close(fd);
    free(buff);

    //write all the file to disk that has been altered
    for(int  j=0;j<cache_size;j++){
      if(dirty_bit[j]==1){
        int w_fd = open(cache_filename[j].c_str(),O_WRONLY);
        if(errno!=EACCES){
          write(w_fd,cache[j].c_str(),strlen(cache[j].c_str()));
        }
        dirty_bit[j]=0;
        close(w_fd);
      }
    }
  }
}

int main(int argc, char *argv[]){
  int server_fd,new_socket,valread,port;
  struct sockaddr_in address;
  struct hostent *addread;
  const char* address_id;
  int opt =1;
  int addrlen = sizeof(address);
  char buffer[4096]={0}; //4 Kib buffer for header

  while((opt=getopt(argc,argv,"cl:"))!=-1){
    switch (opt) {
      case 'c':
        cache_on=true;
        break;
      case 'l':
        log_write=true;
        logfile=optarg;
        break;
      case '?':
              perror("Unknown Flag: EXIT FAILURE");
              exit(EXIT_FAILURE);
              break;
    }
  }

  int extra_arg = 0;
  for(; optind < argc; optind++){
      if(extra_arg==0){
       address_id=argv[optind];
      }
      if(extra_arg==1){
        port = atoi(argv[optind]);
      }
      extra_arg++;
   }

   // if there is too many arguments
   if(argc>6||extra_arg>2){
     perror("USAGE: ./httpserver [-c] [-l] [logfile] [address] [port]");
     exit(EXIT_FAILURE);
   }

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

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; //binds to all avalible addresses

  //Check if address id is valid
  if(extra_arg>0){
    addread =gethostbyname(address_id);
    if(addread==NULL){
      perror("Invalid Address");
      exit(EXIT_FAILURE);
    }
    //if there is a third argument set port to third argument
    if(extra_arg>1){
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
    char *end_log= (char *)"========\n";


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
        char *fail=(char *)"HTTP/1.1 500 Internal Server Error\r\n";
        send(new_socket,fail,strlen(fail),0);

        //log the fail header
        if(log_write){
          int l_fd = open(logfile,O_CREAT|O_WRONLY);
          char* space = (char *)(" ");
          char* header1 = (char *)("FAIL: ");
          char* header2 = (char *)(" HTTP/1.1 --- response 500\n");

          pwrite(l_fd,header1,strlen(header1),offset);
          offset += strlen(header1);
          pwrite(l_fd,op.c_str(),strlen(op.c_str()),offset);
          offset+=strlen(op.c_str());
          pwrite(l_fd,space,strlen(space),offset);
          offset += strlen(space);
          pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),offset);
          offset+=strlen(filename.c_str());
          pwrite(l_fd,header2,strlen(header2),offset);
          offset += strlen(header2);
          pwrite(l_fd,end_log,strlen(end_log),offset);
          offset+=strlen(end_log);
          close(l_fd);
        }

      }
    }else{//If filename is not valid
      char *fail=(char *)"HTTP/1.1 400 Bad Request\r\n";
      send(new_socket,fail,strlen(fail),0);

      //log the fail header
      if(log_write){
        int l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* space = (char *)(" ");
        char* header1 = (char *)("FAIL: ");
        char* header2 = (char *)(" HTTP/1.1 --- response 400\n");

        pwrite(l_fd,header1,strlen(header1),offset);
        offset += strlen(header1);
        pwrite(l_fd,op.c_str(),strlen(op.c_str()),offset);
        offset+=strlen(op.c_str());
        pwrite(l_fd,space,strlen(space),offset);
        offset += strlen(space);
        pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),offset);
        offset+=strlen(filename.c_str());
        pwrite(l_fd,header2,strlen(header2),offset);
        offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),offset);
        offset+=strlen(end_log);
        close(l_fd);
      }
    }
    close(new_socket);
  }
  close(server_fd);
  return 0;
}
