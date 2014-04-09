/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <fcntl.h>
#include <signal.h>
#include <termios.h>

void *fun(void *);
char mostRecent[500];
int end = 0;

int start_server(int PORT_NUMBER)
{

      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	perror("Socket");
	exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
	perror("Setsockopt");
	exit(1);
      }

      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
	perror("Unable to bind");
	exit(1);
      }

      // 3. listen: indicates that we want to listn to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 5) == -1) {
	perror("Listen");
	exit(1);
      }
          
      // once you get here, the server is set up and about to start listening
      printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
      fflush(stdout);
     
while(end == 0){
      // 4. accept: wait here until we get a connection on that port
      int sin_size = sizeof(struct sockaddr_in);
      int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
      printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
      
      // buffer to read data into
      char request[1024];
      
      // 5. recv: read incoming message into buffer
      int bytes_received = recv(fd,request,1024,0);
      // null-terminate the string
      request[bytes_received] = '\0';
      printf("Here comes the message:\n");
      printf("%s\n", request);


      
      // this is the message that we'll send back
      /* it actually looks like this:
        {
           "name": "cit595"
        }
      */
      char * token;
      token = strtok(request, " ");
      while(strcmp(token, "GET") != 0) {
        token = strtok(NULL, " ");
      }
      token = strtok(NULL, " ");
      if(strcmp(token, "/temperature") == 0){
      char temp[500];
      sprintf(temp, "{\n\"temperature\": \"%s\"\n}\n", mostRecent);
      char *reply = temp;
      
      // 6. send: send the message over the socket
      // note that the second argument is a char*, and the third is the number of chars
      send(fd, reply, strlen(reply), 0);
      //printf("Server sent message: %s\n", reply);
}
      // 7. close: close the socket connection
      close(fd);
    }
      close(sock);
      printf("Server closed connection\n");
  
      return 0;
} 

void* fun(void*p){
  //locker = malloc(sizeof(pthread_mutex_t));
  //if (locker == NULL) return 1;
  //pthread_mutex_init(locker,NULL);
  
  /* Connect with Arduino */
  // first, open the connection
  int fd = open("/dev/cu.usbmodem1411", O_RDWR);
  
  // then configure it
  struct termios options;
  tcgetattr(fd, &options);
  cfsetispeed(&options, 9600);
  cfsetospeed(&options, 9600);
  tcsetattr(fd, TCSANOW, &options);
  


  
  char buf[500];
  char buf2[2];
  while (1) {
          
              read(fd, buf2, 1);
              if(buf2[0] != '\n') strcat(buf, buf2);
              //printf("HELLO");
              if(buf2[0] == '\n'){
              char temp[500];
              strcpy(temp, buf);
              if(strcmp(temp, mostRecent) != 0){
                  strcpy(mostRecent, buf);
                  //printf("%s", mostRecent);
        }
      strcpy(buf, "");
              }
          }
}

int main(int argc, char *argv[])
{
  // check the number of arguments
  if (argc != 2)
    {
      printf("\nUsage: server [port_number]\n");
      exit(0);
    }


  int PORT_NUMBER = atoi(argv[1]);
  pthread_t thread_id; // id of thread that will be created
 // create/start the thread 
 pthread_create(&thread_id, NULL, &fun, NULL); 

  start_server(PORT_NUMBER);
}
