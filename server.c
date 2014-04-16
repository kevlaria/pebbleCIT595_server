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
#define RETRY_COUNT 10 // Number of times to retry before deciding that Arduino isn't responding

// Internal stats methods
void update_stats(double);
void insert_into_array(double);

// Server response methods
void send_temperature_to_pebble();
void send_success_header();
void send_failure_to_connect_to_sensor();

// Arduino-related methods
void *start_arduino(void *);
void change_arduino_display_to_c_f();
void change_arduino_standby(int);



int end = 0;
pthread_mutex_t* locker;
int setF = 0;
int fdArduino;
int fdServer;

// Temperature stats
double max;
double min;
double average;
double most_recent;
int count;
double temperature_readings[SECONDS_IN_HOUR]; 
int initial_count_buffer = 0;


/****************************
* SERVER-RELATED CODE
*****************************
*/

/*
* Function to start and run server
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
      fdServer = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
      printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
      // buffer to read data into
      char request[1024];
      
      // 5. recv: read incoming message into buffer
      int bytes_received = recv(fdServer,request,1024,0);

      if (bytes_received == 0) continue;

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
        send_success_header();
        send_temperature_to_pebble();
        //printf("Server sent message: %s\n", reply);
      }

      else if (strcmp(token, "/setF") == 0){
        setF = 1;
        change_arduino_display_to_c_f();
        send_success_header();
      }
      
      else if (strcmp(token, "/setC") == 0){
        setF = 0; 
        change_arduino_display_to_c_f();
        send_success_header();
      }

      else if (strcmp(token, "/standby_0") == 0){    // Turn on standby
        change_arduino_standby(0);
      }

      else if (strcmp(token, "/standby_1") == 0){    // Turn off standby
        change_arduino_standby(1);
      }

      close(fdServer);

      // end of call

    }

    // 7. close: close the socket connection

    close(sock);
    printf("Server closed connection\n");
  
    return 0;
}


/*
* Function to send success header to the phone
*/
void send_success_header(){

  char *reply = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
      
    // 6. send: send the message over the socket
        // note that the second argument is a char*, and the third is the number of chars
  send(fdServer, reply, strlen(reply), 0);
}

/*
* Function to send temperature to phone
*/
void send_temperature_to_pebble(){
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
  if (setF == 0){   // 
    pthread_mutex_lock(locker);

    sprintf(string_to_send_to_phone, "{\n\"mode\": \"Celsius\",\n\"data\": [\n{\"avg\":%.1f,\n\"min\":%.1f,\n\"max\":%.1f,\n\"now\":%.1f\n}\n]\n}", average, min, max, most_recent);

    pthread_mutex_unlock(locker);

    char *reply = string_to_send_to_phone;
      
    // 6. send: send the message over the socket
        // note that the second argument is a char*, and the third is the number of chars
    send(fdServer, reply, strlen(reply), 0);
  } else {  // Send in Fahrenheit

    pthread_mutex_lock(locker);

    double averageF = average * 9/5 + 32;
    double maxF = max * 9/5 + 32;
    double minF = min * 9/5 + 32;
    double most_recentF = most_recent * 9/5 + 32;

    pthread_mutex_unlock(locker);

    sprintf(string_to_send_to_phone, "{\n\"mode\": \"Fahrenheit\",\n\"data\": [\n{\"avg\":%.1f,\n\"min\":%.1f,\n\"max\":%.1f,\n\"now\":%.1f\n}\n]\n}", averageF, minF, maxF, most_recentF);


    char *reply = string_to_send_to_phone;
      
    // 6. send: send the message over the socket
        // note that the second argument is a char*, and the third is the number of chars
    send(fdServer, reply, strlen(reply), 0);

  }
}

/*
* Function to send an error message out
*/
void send_failure_to_connect_to_sensor(){
    char string_to_send_to_phone[500];
    sprintf(string_to_send_to_phone, "{\n\"mode\": \"error\"\n}");
    char *reply = string_to_send_to_phone;
    send(fdServer, reply, strlen(reply), 0);
}


/****************************
* TEMPERATURE STAT-RELATED CODE
*****************************
*/

/*
*  Function to update stats
*/
void update_stats(double new_temperature){

      initial_count_buffer++;
      if (initial_count_buffer < 10) return; // Ignore the first 10 readings

      if (new_temperature > 200) return;  // Don't update stats if temperature is faulty
      
      if (count <= SECONDS_IN_HOUR){
        count++;    // Increment count per update_stats, up to SECONDS_IN_HOUR.
      }

      int index = 0;
      insert_into_array(new_temperature);

      pthread_mutex_lock(locker);
      most_recent = new_temperature; // Udpate most_recent with the current temperature
      max = -500;
      min = 500;
      double total_temperature; // for calculating average


      // Recalculate max, min and average temperature for the past 24 hrs
      for (index = 0; index < count; index ++){
        if (temperature_readings[index] > max) max = temperature_readings[index];
        if (temperature_readings[index] < min) min = temperature_readings[index];
        total_temperature += temperature_readings[index];
      }

      average = total_temperature / (count);

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
*  Function to update array of temperatures
*/
void insert_into_array(double new_temperature){
  int index = 0;

  for (index = SECONDS_IN_HOUR; index > 0; index --){
    temperature_readings[index] = temperature_readings[index - 1];
  }
  temperature_readings[0] = new_temperature;
}


/****************************
* ARUDINO-RELATED CODE
*****************************
*/

/*
*  Function to connect to the arduino
*/
void* start_arduino(void*p){

  
  /* Connect with Arduino */
  // first, open the connection
  
  fdArduino = open("/dev/cu.usbmodem1421", O_RDWR);

  // then configure it
  struct termios options;
  tcgetattr(fdArduino, &options);
  cfsetispeed(&options, 9600);
  cfsetospeed(&options, 9600);
  tcsetattr(fdArduino, TCSANOW, &options);
  
  
  
  char buf[500];
  char buf2[2];



  while (1) {

    int failure_counter = RETRY_COUNT;
    // Read in temperature from Arduino. It will always be sent in Celsius
    int response = read(fdArduino, buf2, 1);
    while (response == 0){
      if (failure_counter <= 0){
        void send_failure_to_connect_to_sensor(); // Failed to connect to sensor > threshold, so sending message to watch
        failure_counter = RETRY_COUNT; // reset failure_counter

      } else {
        failure_counter --;
        response = read(fdArduino, buf2, 1);
      }
    }


    if(buf2[0] != '\n') strcat(buf, buf2); // copy individual letter to buffer, 
    // and continue reading next letter
             
    else {  // Output from arduino is complete. 

      double temperature;
      sscanf(buf, "%lf", &temperature); // Convert number into double

      update_stats(temperature);

      strcpy(buf, ""); // Clear buffer
      
    }
  }
  close(fdArduino);
}


/*
* Method to change the arduino display to display Celsius or Fahrenheit 
*/
void change_arduino_display_to_c_f(){
    if(setF == 1){
      int bytes_written = write(fdArduino, "0", 1);
//      printf("BYTES %d", bytes_written);
    }
    else {
      int bytes_written = write(fdArduino, "1", 1);
//      printf("BYTES %d", bytes_written);
    }
}


/*
* Method to tell arduino to go in / out of standby mode
*/
void change_arduino_standby(int signal){
  if (signal == 0){
    int bytes_written = write(fdArduino, "o", 1); // Signal to turn on standby
  } else if (signal == 1){
    int bytes_written = write(fdArduino, "O", 1); // Signal to turn off standby 
  }
}


/**************
*  MAIN FUNCTION
*************
*/
int main(int argc, char *argv[])
{
  locker = malloc(sizeof(pthread_mutex_t)); // Mutex lock for stats
  if (locker == NULL) return 1;
  pthread_mutex_init(locker,NULL);

  
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

  int index = 0;

  for (index = 0; index < SECONDS_IN_HOUR; index ++){
    temperature_readings[index] = 0;
  }


  start_server(PORT_NUMBER);
}
