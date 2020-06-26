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
#include <pthread.h>
#include <semaphore.h>
#define PORT 80

using namespace std;

const size_t BUFF_SIZE =16384;//16 Kib for buffer
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pwrite_mutex = PTHREAD_MUTEX_INITIALIZER;
char* logfile;
bool log_write=false;
int offset =0;
sem_t thread_sem;
size_t thread_in_use=0;

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

struct get_funct_struct{
  int socket;
  const char *filename;
};

//function to handle GET Request
void *get_funct(void* arg){
  struct get_funct_struct *get_struct =
    (struct get_funct_struct*) arg;
  const char *filename = get_struct->filename;
  int socket = get_struct->socket;

  char *end=(char *)"\r\n";
  char *end_log= (char *)"========\n";

  int fd = open(filename,O_RDONLY);
  int l_fd,p_offset=0;

  if(fd==-1){
    if(errno==EACCES){ //if we do not have permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";

      //logs the fail header
      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: GET ");
        char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

        //calculating how much space is needed for the log
        pthread_mutex_lock(&pwrite_mutex);
        p_offset=offset;
        offset+= strlen(header1)+strlen(header2)+
            strlen(filename)+strlen(end_log);
        pthread_mutex_unlock(&pwrite_mutex);


        pwrite(l_fd,header1,strlen(header1),p_offset);
        p_offset += strlen(header1);
        pwrite(l_fd,filename,strlen(filename),p_offset);
        p_offset+=strlen(filename);
        pwrite(l_fd,header2,strlen(header2),p_offset);
        p_offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),p_offset);
        p_offset+=strlen(end_log);
      }

      send(socket,fail,strlen(fail),0);
    }else{ //if file does not exist
      char *fail=(char *)"HTTP/1.1 404 File not found\r\n";

      //logs the fail header
      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: GET ");
        char* header2 = (char *)(" HTTP/1.1 --- response 404\n");

        //calculating how much space is needed for the log
        pthread_mutex_lock(&pwrite_mutex);
        p_offset=offset;
        offset+= strlen(header1)+strlen(header2)+
            strlen(filename)+strlen(end_log);
        pthread_mutex_unlock(&pwrite_mutex);

        pwrite(l_fd,header1,strlen(header1),p_offset);
        p_offset += strlen(header1);
        pwrite(l_fd,filename,strlen(filename),p_offset);
        p_offset+=strlen(filename);
        pwrite(l_fd,header2,strlen(header2),p_offset);
        p_offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),p_offset);
        p_offset+=strlen(end_log);
      }

      send(socket,fail,strlen(fail),0);
    }
  }else{
    char *buff = (char*)malloc(BUFF_SIZE*sizeof(char));
    size_t content_len=0,length;
    for(;;){
      length=read(fd, buff, sizeof(buff));
      content_len+=length;
      if(length==0) break;
      memset(buff, 0,BUFF_SIZE);
    }

    char *success=(char *)"HTTP/1.1 200 OK\r\nContent-Length: ";
    string content_length=to_string(content_len)+"\r\n";
    send(socket,end,strlen(end),0);

    //Send success message to client
    send(socket,success,strlen(success),0);
    send(socket,content_length.c_str(),strlen(content_length.c_str()),0);
    close(fd);

    //logging the header to logfile
    if(log_write){
      char* header1 = (char *)("GET ");
      char* header2 = (char *)(" length ");

      //calculating how much space is needed for the log
      pthread_mutex_lock(&pwrite_mutex);
      p_offset=offset;
      offset=offset+strlen(header1)+strlen(header2)
        +strlen(content_length.c_str())+strlen(filename)
        +strlen(end_log);
      pthread_mutex_unlock(&pwrite_mutex);

      // print function and filename
      l_fd = open(logfile,O_CREAT|O_WRONLY);
      pwrite(l_fd,header1,strlen(header1),p_offset);
      p_offset += strlen(header1);
      pwrite(l_fd,filename,strlen(filename),p_offset);
      p_offset+=strlen(filename);
      pwrite(l_fd,header2,strlen(header2),p_offset);
      p_offset += strlen(header2);
      pwrite(l_fd,content_length.c_str(),strlen(content_length.c_str()),p_offset);
      p_offset += strlen(content_length.c_str());
      pwrite(l_fd,end_log,strlen(end_log),p_offset);
      p_offset+=strlen(end_log);
    }

    fd = open(filename,O_RDONLY);
    //read in file into buffer and send content to client
    for(;;){
      length=read(fd, buff, BUFF_SIZE*sizeof(char));
      content_len+=length;
      if(length==0) break;
      send(socket,buff,length,0);
      memset(buff, 0,BUFF_SIZE);
    }
    send(socket,end,strlen(end),0);
  }
  close(fd);
  close(socket);
  sem_post(&thread_sem);
  pthread_mutex_lock(&thread_mutex);
  thread_in_use--;
  pthread_mutex_unlock(&thread_mutex);
  return NULL;
}

struct put_funct_struct{
  int socket;
  const char *filename;
  size_t length;
};
//function to handle PUT Request
void* put_funct(void* arg){
  struct put_funct_struct *put_struct =
    (struct put_funct_struct*) arg;
  int socket = put_struct->socket;
  const char *filename = put_struct->filename;
  size_t length = put_struct->length;
  int fd = open(filename,O_WRONLY);
  int l_fd,p_offset;
  int write_access;
  bool created=false;

  char *next_line=(char *)"\n";
  char *end_log= (char *)"========\n";
  char *space= (char *)(" ");

  if(errno==EACCES){//if we do not have permission to the file
    char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";
    //logs the fail header
    if(log_write){
      l_fd = open(logfile,O_CREAT|O_WRONLY);
      char* header1 = (char *)("FAIL: PUT ");
      char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

      //calculating how much space is needed for the log
      pthread_mutex_unlock(&pwrite_mutex);
      p_offset=offset;
      offset+= strlen(header1)+strlen(header2)+
          strlen(filename)+strlen(end_log);
      pthread_mutex_unlock(&pwrite_mutex);

      pwrite(l_fd,header1,strlen(header1),p_offset);
      p_offset += strlen(header1);
      pwrite(l_fd,filename,strlen(filename),p_offset);
      p_offset+=strlen(filename);
      pwrite(l_fd,header2,strlen(header2),p_offset);
      p_offset += strlen(header2);
      pwrite(l_fd,end_log,strlen(end_log),p_offset);
      p_offset+=strlen(end_log);
    }
    send(socket,fail,strlen(fail),0);
  }else{
    if(fd==-1){
      close(fd);
      fd = open(filename,O_CREAT|O_WRONLY);
      created=true;
    }
    //recv content_length amount of data from client
    char *buff = (char*)malloc(length*sizeof(char));
    recv(socket, buff, length,0);
    write_access=write(fd,buff,length);

    if(log_write&&write_access!=-1){
      string con_len = to_string(length);
      char* header1 = (char *)("PUT ");
      char* header2 = (char *)(" length ");
      char bytecount[8],hex[2];
      size_t byte =0;
      sprintf(bytecount,"%08zu",byte);

      //calculating how much space is needed for the log
      pthread_mutex_lock(&pwrite_mutex);
      p_offset=offset;
      offset=offset+strlen(header1)+strlen(header2)+70*(length/20)
        +strlen(con_len.c_str())+strlen(filename)
        +strlen(end_log)+strlen(next_line);
      if(length%20!=0){
        offset+=10;
        offset+=(length%20)*3;
      }
      pthread_mutex_unlock(&pwrite_mutex);

      // logs the PUT header
      l_fd = open(logfile,O_CREAT|O_WRONLY);
      pwrite(l_fd,header1,strlen(header1),p_offset);
      p_offset += strlen(header1);
      pwrite(l_fd,filename,strlen(filename),p_offset);
      p_offset+=strlen(filename);
      pwrite(l_fd,header2,strlen(header2),p_offset);
      p_offset += strlen(header2);
      pwrite(l_fd,con_len.c_str(),strlen(con_len.c_str()),p_offset);
      p_offset += strlen(con_len.c_str());
      pwrite(l_fd,next_line,strlen(next_line),p_offset);
      p_offset+=strlen(next_line);

      //logs the content of the file
      for(int i =0; i<(int)length; i++){
        if(i%20==0){
           if(i!=0){
              pwrite(l_fd,next_line,strlen(next_line),p_offset);
              p_offset+=strlen(next_line);
           }

          pwrite(l_fd,bytecount,strlen(bytecount),p_offset);
          p_offset+=strlen(bytecount);
          pwrite(l_fd,space,strlen(space),p_offset);
          p_offset+=strlen(space);
          byte+= 20;
	  sprintf(bytecount,"%08zu",byte);

        }

        sprintf(hex,"%02X ",buff[i]);
        pwrite(l_fd,hex,strlen(hex),p_offset);
        p_offset+=strlen(hex);
      }
      pwrite(l_fd,next_line,strlen(next_line),p_offset);
      p_offset+=strlen(next_line);
      pwrite(l_fd,end_log,strlen(end_log),p_offset);
      p_offset+=strlen(end_log);
      close(l_fd);
    }

    if(created){//Send 201 code if we have created a file
      char *success=(char *)"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n";
      send(socket,success,strlen(success),0);
    }else if(write_access==-1){
      //if we do not have write permission to the file
      char *fail=(char *)"HTTP/1.1 403 Forbidden\r\n";

      if(log_write){
        l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* header1 = (char *)("FAIL: PUT ");
        char* header2 = (char *)(" HTTP/1.1 --- response 403\n");

        pthread_mutex_unlock(&pwrite_mutex);
        p_offset=offset;
        offset+= strlen(header1)+strlen(header2)+
            strlen(filename)+strlen(end_log);
        pthread_mutex_unlock(&pwrite_mutex);

        pwrite(l_fd,header1,strlen(header1),p_offset);
        p_offset += strlen(header1);
        pwrite(l_fd,filename,strlen(filename),p_offset);
        p_offset+=strlen(filename);
        pwrite(l_fd,header2,strlen(header2),p_offset);
        p_offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),p_offset);
        p_offset+=strlen(end_log);
      }
      send(socket,fail,strlen(fail),0);
    }else{//send success message to client
      char *success=(char *)"HTTP/1.1 200 OK\r\n";
      send(socket,success,strlen(success),0);
    }
    close(fd);
  }
  sem_post(&thread_sem);
  pthread_mutex_lock(&thread_mutex);
  thread_in_use--;
  pthread_mutex_unlock(&thread_mutex);
  close(socket);
  return NULL;
}

//Main Function
int main(int argc, char *argv[]){
  int server_fd,new_socket,valread,port;
  struct sockaddr_in address;
  struct hostent *addread;
  int opt;
  int addrlen = sizeof(address);
  const char* address_id;
  size_t num_of_threads = 4; //default number of threads
  char buffer[4096]={0}; //4 Kib buffer for header

  while((opt=getopt(argc,argv,"N:l:"))!=-1){
    switch (opt) {
      case 'N':
        num_of_threads= atoi(optarg);
        break;
      case 'l':
        log_write=true;
        logfile=optarg;
        int l_file;
        if((l_file=open(logfile,O_WRONLY))>0){
          remove(logfile);
          close(l_file);
        }
        break;
      case '?':
              perror("Unknown Flag: EXIT FAILURE");
              exit(EXIT_FAILURE);
              break;
    }
  }

  //create the pool of workers and initialize the semaphore
  pthread_t *thread_pool= new pthread_t[num_of_threads];
  sem_init(&thread_sem, 0, num_of_threads);

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
   if(argc>7||extra_arg>2){
     perror("USAGE: ./httpserver [-N] [num_of_thread] [-l] [logfile] [address] [port]");
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
  //If there is a second argument set check if it is valid

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
    size_t content_len=0;
    int p_offset;
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

        //setting the arguments for the worker threads
        struct get_funct_struct arg;
        arg.socket = new_socket;
        arg.filename = filename.c_str();

        //call sem down, if no avalible workers block
        sem_wait(&thread_sem);
        for(size_t j = 0; j<num_of_threads;j++){
          pthread_mutex_lock(&thread_mutex);
          thread_in_use++;
          pthread_mutex_unlock(&thread_mutex);
          if(pthread_create(&thread_pool[j],NULL,get_funct,&arg)!=-1){
            break;
         }
       }
      }else if(op.compare("PUT")==0){//if Request is a PUT
        //Parses the content length from the header
        while (content_header.compare("Content-Length:")!=0) {
          content_header=parse_header(buffer,i);
          i+=content_header.length()+1;
        }
        //setting the arguments for the worker threads
        content_len=stoi(parse_header(buffer,i));
        struct put_funct_struct arg;
        arg.socket = new_socket;
        arg.filename = filename.c_str();
        arg.length = content_len;
        content_len=stoi(parse_header(buffer,i));

        //call sem down, if no avalible workers block
        sem_wait(&thread_sem);
        for(size_t j = 0; j<num_of_threads;j++){
          pthread_mutex_lock(&thread_mutex);
          thread_in_use++;
          pthread_mutex_unlock(&thread_mutex);
          if(pthread_create(&thread_pool[j],NULL,put_funct,&arg)!=-1){
           break;
          }
       }
      }else{//If if put is not GET or PUT
        char *fail=(char *)"HTTP/1.1 500 Internal Server Error\r\n";

        //log the fail header
        if(log_write){
          int l_fd = open(logfile,O_CREAT|O_WRONLY);
          char* space = (char *)(" ");
          char* header1 = (char *)("FAIL: ");
          char* header2 = (char *)(" HTTP/1.1 --- response 500\n");

          //calculating how much space is needed for the log
          pthread_mutex_lock(&pwrite_mutex);
          p_offset=offset;
          offset+= strlen(header1)+strlen(header2)+strlen(filename.c_str())+
                   strlen(end_log)+strlen(space)+strlen(op.c_str());
          pthread_mutex_unlock(&pwrite_mutex);

          pwrite(l_fd,header1,strlen(header1),p_offset);
          p_offset += strlen(header1);
          pwrite(l_fd,op.c_str(),strlen(op.c_str()),p_offset);
          p_offset+=strlen(op.c_str());
          pwrite(l_fd,space,strlen(space),p_offset);
          p_offset += strlen(space);
          pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),p_offset);
          p_offset+=strlen(filename.c_str());
          pwrite(l_fd,header2,strlen(header2),p_offset);
          p_offset += strlen(header2);
          pwrite(l_fd,end_log,strlen(end_log),p_offset);
          p_offset+=strlen(end_log);
        }

        send(new_socket,fail,strlen(fail),0);
        close(new_socket);
      }
    }else{//If filename is not valid
      char *fail=(char *)"HTTP/1.1 400 Bad Request\r\n";

      //log the fail header
      if(log_write){
        int l_fd = open(logfile,O_CREAT|O_WRONLY);
        char* space = (char *)(" ");
        char* header1 = (char *)("FAIL: ");
        char* header2 = (char *)(" HTTP/1.1 --- response 400\n");

        //calculating how much space is needed for the log
        pthread_mutex_lock(&pwrite_mutex);
        p_offset=offset;
        offset+= strlen(header1)+strlen(header2)+strlen(filename.c_str())+
                 strlen(end_log)+strlen(space)+strlen(op.c_str());
        pthread_mutex_unlock(&pwrite_mutex);

        pwrite(l_fd,header1,strlen(header1),p_offset);
        p_offset += strlen(header1);
        pwrite(l_fd,op.c_str(),strlen(op.c_str()),p_offset);
        p_offset+=strlen(op.c_str());
        pwrite(l_fd,space,strlen(space),p_offset);
        p_offset += strlen(space);
        pwrite(l_fd,filename.c_str(),strlen(filename.c_str()),p_offset);
        p_offset+=strlen(filename.c_str());
        pwrite(l_fd,header2,strlen(header2),p_offset);
        p_offset += strlen(header2);
        pwrite(l_fd,end_log,strlen(end_log),p_offset);
        p_offset+=strlen(end_log);
      }
      send(new_socket,fail,strlen(fail),0);
      // close(new_socket);
    }

    for(size_t j = 0; j < thread_in_use;j++){
      pthread_join(thread_pool[j],NULL);
    }
    close(new_socket);
  }
  close(server_fd);
  return 0;
}
