/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */
 
 // TODO BUG with SETC & SETF - interfacing between arduino thread and server code. Reading something in from server, trying to translate to server to thread in arduino server. Stuck in F after transition or not transitioning

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
#define SECONDS_IN_HOUR 86400 // 86,400 = 24 hours in seconds

void *start_arduino(void *);
void update_stats(double);
void insert_into_array(double);
char mostRecent[500];
int end = 0;
int count = 0;
pthread_mutex_t* locker;
pthread_mutex_t* insert_into_array_lock;
int setting = 0;
int setF = 0;
int setC = 1;

// Temperature stats
double max;
double min;
double average;
double most_recent;
int count;
double temperature_readings[SECONDS_IN_HOUR]; 

/*
* CODE TO RUN SERVER
*/
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
      setting = 0;
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
      /*
      * This is where the server receives message from the phone. The message will be in the following formats:
	- "/temperature"
	- "/pause" (to be implemented)
	- "/resume" (to be implemented)
     
      */
      
      char * token;
      token = strtok(request, " ");
      while(strcmp(token, "GET") != 0) {
        token = strtok(NULL, " ");
      }
      token = strtok(NULL, " ");
      printf("THIS IS TOKEN %s", token);
      if(strcmp(token, "/temperature") == 0){ 
      char string_to_send_to_phone[500];

      /* Update string to be sent to phone.  * The server should have the following format:
        {
          "mode": "Celsius",
          "data": [
           {"avg": 25.5,
           "min": 10.5,
           "max": 38.4,
           "now": 30.9}
           ]
         } 
    */
      pthread_mutex_lock(locker);

      sprintf(string_to_send_to_phone, "{\n\"mode\": \"Celsius\",\n\"data\": [\n{\"avg\":%.2f,\n\"min\":%.2f,\n\"max\":%.2f,\n\"now\":%.2f\n}\n]\n}", average, min, max, most_recent);

      pthread_mutex_unlock(locker);

      char *reply = string_to_send_to_phone;
      
      // 6. send: send the message over the socket
      // note that the second argument is a char*, and the third is the number of chars
      send(fd, reply, strlen(reply), 0);
      //printf("Server sent message: %s\n", reply);
}

else if (strcmp(token, "/setF") == 0){
  setting = 1;
    setC = 0;
    setF = 1;
    char temp[500];
      sprintf(temp, "{\n\"temperature\": \"%s\"\n}\n", mostRecent);
      char *reply = temp;
      
      // 6. send: send the message over the socket
      // note that the second argument is a char*, and the third is the number of chars
      send(fd, reply, strlen(reply), 0);
  
}
else if (strcmp(token, "/setC") == 0){
  setting = 1;
  setC = 1;
  setF = 0; 
  char temp[500];
      sprintf(temp, "{\n\"temperature\": \"%s\"\n}\n", mostRecent);
      char *reply = temp;
      
      // 6. send: send the message over the socket
      // note that the second argument is a char*, and the third is the number of chars
      send(fd, reply, strlen(reply), 0);
}
      // 7. close: close the socket connection
      close(fd);
}
      close(sock);
      printf("Server closed connection\n");
  
      return 0;
} 


/*
* CODE TO CONNECT TO ARUDINO
*/
void* start_arduino(void*p){

  
  /* Connect with Arduino */
  // first, open the connection
  
  int fd = open("/dev/cu.usbmodem1421", O_RDWR);

  // then configure it
  struct termios options;
  tcgetattr(fd, &options);
  cfsetispeed(&options, 9600);
  cfsetospeed(&options, 9600);
  tcsetattr(fd, TCSANOW, &options);
  

  
  char buf[500];
  char buf2[2];
  while (1) {
    if(setF == 1){
      int bytes_written = write(fd, "0", 1);
      printf("BYTES %d", bytes_written);
    }
    else if(setC == 0){
      int bytes_written = write(fd, "1", 1);
      printf("BYTES %d", bytes_written);
    }

    // Read in temperature from Arduino. 
    // if setting is not 1 (ie if token is neither setF nor setC)

    if(setting != 1){
      read(fd, buf2, 1);
      if(buf2[0] != '\n') strcat(buf, buf2); // copy individual letter to buffer, 
      // and continue reading next letter
             
      else {  // Output from arduino is complete. 
        double temperature;
        sscanf(buf, "%lf", &temperature); // Convert number into double

        update_stats(temperature);

        strcpy(buf, ""); // Clear buffer
      }
    }
  }
}

/*
*  CODE TO UPDATE STATS
*/
void update_stats(double new_temperature){

      if (new_temperature > 200) return;  // Don't update stats if temperature is faulty

      int index = 0;
      insert_into_array(new_temperature);

      pthread_mutex_lock(locker);
      most_recent = new_temperature; // Udpate most_recent with the current temperature
      max = -500;
      min = 500;
      double total_temperature; // for calculating average

      if (count <= SECONDS_IN_HOUR){
        count++;    // Increment count per update_stats, up to SECONDS_IN_HOUR.
      }

      // Recalculate max, min and average temperature for the past 24 hrs
      for (index = 0; index < count; index ++){
        if (temperature_readings[index] > max) max = temperature_readings[index];
        if (temperature_readings[index] < min) min = temperature_readings[index];
        total_temperature += temperature_readings[index];
      }

      average = total_temperature / count;

      pthread_mutex_unlock(locker);

/*
      printf("\n%.2f <--- max\t", max);
      printf("\n%.2f <--- min\t", min);
      printf("\n%.2f <--- total\t", total_temperature);
      printf("\n%.2f <--- avg\t", average);
      printf("\n");

      for (index = 0; index < 10; index ++){
        printf("%.2f\t", temperature_readings[index]);
      }
      printf("\n");
*/

}

/*
*  CODE TO UPDATE ARRAY OF TEMPERATURES
*/
void insert_into_array(double new_temperature){
  int index = 0;

  for (index = SECONDS_IN_HOUR; index > 0; index --){
    temperature_readings[index] = temperature_readings[index - 1];
  }
  temperature_readings[0] = new_temperature;
}

/*
*  MAIN FUNCTION
*/
int main(int argc, char *argv[])
{
  locker = malloc(sizeof(pthread_mutex_t)); // Mutex lock for stats
  if (locker == NULL) return 1;
  pthread_mutex_init(locker,NULL);

  insert_into_array_lock = malloc(sizeof(pthread_mutex_t)); // Mutex lock for updating array of all temperatures
  if (insert_into_array_lock == NULL) return 1;
  pthread_mutex_init(insert_into_array_lock,NULL);

  // check the number of arguments
  if (argc != 2)
    {
      printf("\nUsage: server [port_number]\n");
      exit(0);
    }


  int PORT_NUMBER = atoi(argv[1]);
  pthread_t thread_id; // id of thread that will be created
 // create/start the thread 
 pthread_create(&thread_id, NULL, &start_arduino, NULL); 

  start_server(PORT_NUMBER);
}
