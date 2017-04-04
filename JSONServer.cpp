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
#include <string>
#include <iostream>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include "readUSB.h"
#include <deque>
#include <pthread.h>
using namespace std;

double avg;
double low;
double high;
double num;

void calculate_data(deque<double> temp_queue) {
    
    double sum = 0;
    int count = 0;
    deque<double>::iterator it = temp_queue.begin();
    low = *it;
    high = *it;
    
    while(it != temp_queue.end()) {
        if(*it != 0) {
            sum += *it;
            if(*it - high > 0) {
                high = *it;
            }
            if(*it - low < 0) {
                low = *it;
            }
            count++;
        }
        *it++;
    }
    
    avg = sum / count;
}

void* read_arduino(void* p) {
    int* arduino_fd = (int*)p;
    configure(*arduino_fd);
    char buf[100];
    int start = 0;
    int bytes_read = read(*arduino_fd, buf, 100);
    int end = bytes_read;
    string message;
    
    deque<double> temp_queue;
    
    while(true) {
        int j = start;
        
        while(j < end) {
            if(buf[j] == '\n') {
                break;
            }
            message += buf[j];
            j++;
        }
        
        if(j < end) {
            if(message.length() != 0) {
                if(sscanf(message.c_str(), "The temperature is %lf degree C\n", &num) > 0){
                    temp_queue.push_back(num);
                }
                
                // only keep 3600 records (1 record per second, 3600 records per hour)
                if(temp_queue.size() > 3600){
                    temp_queue.pop_front();
                }
                
                if(!temp_queue.empty()){
                    calculate_data(temp_queue);
                    cout << "Avg: " << avg << " low: " << low << " high: " << high << endl;
                }
                // Data* cur_data = calculate_data(temp_queue);
                
                
            }
            message.clear();
            start = ++j;
        } else {
            bytes_read = read(*arduino_fd, buf, 100);
            start = 0;
            end = bytes_read;
        }
    }
}

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
    cout << endl << "Server configured to listen on port " << PORT_NUMBER << endl;
    fflush(stdout);
    
    
    while(true){
        
        // 4. accept: wait here until we get a connection on that port
        int sin_size = sizeof(struct sockaddr_in);
        int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
        cout << "Server got a connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;
        
        // buffer to read data into
        char request[1024];
        
        // 5. recv: read incoming message into buffer
        int bytes_received = recv(fd,request,1024,0);
        // null-terminate the string
        request[bytes_received] = '\0';
        cout << "Here comes the message:" << endl;
        cout << request << endl;
        
        string reply = "{\n\"temp\": \""+ to_string(num) +"\",\n\"avg\": \"" + to_string(avg) + "\",\n\"low\": \"" + to_string(low) + "\",\n\"high\": \"" + to_string(high) + "\"\n}\n";
        cout << reply << endl;
        send(fd, reply.c_str(), reply.length(), 0);
        close(fd);
    }
    
    close(sock);
    cout << "Server closed connection" << endl;
    
    return 0;
}



int main(int argc, char *argv[])
{
    // check the number of arguments
    // argc2:port number ; argc: equip
    if (argc != 3)
    {
        cout << endl << "Usage: ./server [port_number] [name of the serial port (USB) device file]" << endl;
        exit(0);
    }
    
    int PORT_NUMBER = atoi(argv[1]);
    
    // get the name from the command line
    char* file_name = argv[2];
    
    // try to open the file for reading and writing
    int arduino_fd = open(argv[2], O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (arduino_fd < 0) {
        perror("Could not open file");
        exit(1);
    }
    else {
        cout << "Successfully opened " << argv[2] << " for reading/writing" << endl;
    }
    
    int r = 0;
    pthread_t arduino_thread;
    r = pthread_create(&arduino_thread, NULL, read_arduino, &arduino_fd);
    start_server(PORT_NUMBER);
    void* r1;
    pthread_join(arduino_thread, &r1);
}

