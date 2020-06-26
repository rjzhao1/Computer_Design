//Ricky Zhao
//rjzhao@ucsc.edu
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<err.h>
#include<errno.h>
#include<sys/stat.h>


//Declare a constant size of 32Kib for buffer
const size_t BUFF_SIZE =32768;

//Write functions taking a filename as argument
//creates buffer,read file a block at a time with buffer
//,write buffer to stdout
void write_file(size_t filename){
   char *buff = (char*)malloc(BUFF_SIZE*sizeof(char));
   size_t length;
   for(;;){
      length=read(filename, buff, sizeof(buff));
      if(length==0) break;
      write(1,buff,length);
      memset(buff, 0,BUFF_SIZE*sizeof(char));
   }
   free(buff);
}

int main(int argc, char *argv[]){
   //Case: if there is no argument,write from stdin
   if(argc<2){
      write_file(STDIN_FILENO);
   }
   //Loops through all input
   for(size_t i = 1;i<size_t(argc);i++){
      //If the input is "-",write from stdin
      if(strcmp(argv[i],"-")==0){
         write_file(STDIN_FILENO);
      }else{
         struct stat check;
         stat(argv[i],&check);
         //check if the file is a directory
         if(check.st_mode & S_IFDIR){
            warnx("%s: Is a directory",argv[i]);
         }else{
            int fd = open(argv[i],O_RDONLY);
            if(fd==-1){
               if(errno==EACCES){ // if input does not have Permission
                  warnx("%s: Permission denied",argv[i]);
               }else{ // input does not exist
                  warnx("%s: No such file or directory",argv[i]);
               }
            }else{  //input is afile name
               write_file(fd);
            }
            close(fd);
         }
      }
   }
}
