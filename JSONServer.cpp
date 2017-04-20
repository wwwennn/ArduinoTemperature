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
#include <deque>
#include <pthread.h>
using namespace std;

// global variables
int arduino_fd;
int PORT_NUMBER;
double num = 0;
deque<double> temp_queue;
double avg = 0;
double low = 0;
double high = 0;
bool arduino_status = false; // false: arduino is not working true: arduino is working
char threshold[20];
char user_input_temp[20];
// 0419 stop server
const char* file_name;
pthread_mutex_t lock1;
bool server_status = true; // false: server is not working, true: working

void* start_server(void* p);
void* read_arduino(void*);
void configure(int);
void calculate_data(deque<double>);
void handler(int);
void* stop_arduino(void*);
void* stop_server(void*);
int is_server_open();


int main(int argc, const char * argv[]) {
    // check the number of arguments. port: argv[1], device: argv[2]
    if(argc != 3) {
        cout << endl << "Usage: ./server [port_number] [name of the serial port (USB) device file]" << endl;
        exit(0);
    }
    
    // 0419 stop server
    PORT_NUMBER = atoi(argv[1]);
    file_name = argv[2];
    pthread_t arduino_thread, server_thread;
    pthread_create(&server_thread, NULL, start_server, &arduino_thread);
    
    while(is_server_open()){
        arduino_fd = open(argv[2], O_CREAT | O_RDWR | O_NOCTTY | O_NDELAY);
        cout << "Try to open the " << argv[2] << "..." << endl;
        while(arduino_fd < 0 && is_server_open()) {
            // perror("Could not open file");
            arduino_fd = open(argv[2], O_CREAT | O_RDWR | O_NOCTTY | O_NDELAY);
        }
        cout << "Successfully opened " << argv[2] << " for reading/writing" << endl;
        // update the arduino status
        arduino_status = true;
        // fcntl(arduino_fd, F_SETFL, FNDELAY);
        
        // start related functions
        pthread_create(&arduino_thread, NULL, read_arduino, &arduino_thread);
        cout << "read_arduino is Successfully created!" << endl;
        
        // end of the program
        void* r;
        pthread_join(arduino_thread, &r);
        cout << "arduino_fd is closed now" << endl;
        // close(arduino_fd);
    }
    
    void* r1;
    pthread_join(server_thread, &r1);
    return 0;
}

void* start_server(void* p) {
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
    
    // 0419 stop server
    pthread_mutex_init(&lock1, NULL);
    pthread_t stop_server_thread;
    int r1;
    void* r;
    r1 = pthread_create(&stop_server_thread, NULL, &stop_server, NULL);
    if(r1 == 0){
        cout << "stop_server_thread is successfully created !!! " << endl;
    }else{
        cout << "stop_server_thread is not created !!! " << endl;
    }
    
    while(is_server_open()) {
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
        char method[20];
        char url[20];
        sscanf(request, "%s %s", method, url);
        cout << "url:" << url << endl;
        
        string reply;
        // /convert/c means conversion happens while we want result in c
        if(strcmp(url, "/c") == 0 || strcmp(url, "/f") == 0 || strcmp(url, "/convert/c") == 0 || strcmp(url, "/convert/f") == 0) {
            // 0412
            if(!arduino_status){
                // 0419
                reply = "{\n\"temp\": \"arduino is not working\"\n}\n";
                cout << reply << endl;
                send(fd, reply.c_str(), reply.length(), 0);
                close(fd);
                continue;
            }
            calculate_data(temp_queue);
            // 0412
            cout << "Avg: " << avg << " low: " << low << " high: " << high << endl;
            int i = 0;
            char temp_char = url[i];
            while(temp_char != '\0'){
                i++;
                temp_char = url[i];
            }
            
            if(url[i-1] == 'c'){
                reply = "{\n\"temp\": \""+ to_string(num) +"\",\n\"avg\": \"" + to_string(avg) + "\",\n\"low\": \"" + to_string(low) + "\",\n\"high\": \"" + to_string(high) + "\"\n}\n";
            }else{
                reply = "{\n\"temp\": \""+ to_string(1.8*num+32) +"\",\n\"avg\": \"" + to_string(1.8*avg+32) + "\",\n\"low\": \"" + to_string(1.8*low+32) + "\",\n\"high\": \"" + to_string(1.8*high+32) + "\"\n}\n";
                
            }
            char f = url[i-1];
            cout << "f: " << f << endl;
            char *char_ptr = &f;
            int bytes_written = write(arduino_fd, char_ptr, 1);
            cout <<bytes_written<<endl;
            perror("error");
            if(bytes_written == -1){
                reply = "{\n\"temp\": \"arduino is not working\"\n}\n";
            }
            
        } else if(strcmp(url, "/standby") == 0) {
            reply = "{\n\"msg\": \"Standby Mode\"\n}";
            char standby = 'y';
            char* char_ptr = &standby;
            int bytes_written = write(arduino_fd, char_ptr, 1);
            cout <<bytes_written<<endl;
            perror("error");
        } else if(strcmp(url, "/pinWindow1") == 0) {
            reply = "{\n\"msg\": \"pin_window1\"\n}";
            
        } else if(strcmp(url, "/pinWindow2") == 0){
            reply = "{\n\"msg\": \"pin_window2\"\n}";
            
        } else if(sscanf(url, "/pin2/%s", threshold) > 0){
            if(!arduino_status){
                reply = "{\n\"msg\": \"can not set threshold\"\n}\n";
                cout << reply << endl;
                send(fd, reply.c_str(), reply.length(), 0);
                close(fd);
                continue;
            }
            cout << "threshold: " << threshold << endl;
            reply = "{\n\"msg\": \"threshold received\",\n\"temp\": \"" + to_string(num) +"\",\n\"threshold\": \"" + threshold;
            reply += "\"\n}";
            
        } else if(sscanf(url, "/%s", user_input_temp) > 0) {
            // 0412
            if(!arduino_status){
                reply = "{\n\"msg\": \"can not set temperature\"\n}\n";
                cout << reply << endl;
                send(fd, reply.c_str(), reply.length(), 0);
                close(fd);
                continue;
            }
            reply = "{\n\"msg\": \"temp received\",\n\"temp\": \"";
            reply += user_input_temp;
            reply += "\"\n}";
            int k = 0;
            while(url[k] != '\0'){
                k++;
            }
            // for(k = 6; url[k] != '\0'; k++) {
            //     reply += url[k];
            // }
            url[0] = 'w';
            int bytes_written = write(arduino_fd, url, k);
            // cout <<bytes_written<<endl;
            if(bytes_written == -1){
                reply = "{\n\"msg\": \"can not set temperature\"\n}\n";
            }
            // perror("error");
            
        } else{
            reply = "{\n\"msg\": \"not valid url\"\n}\n";
            cout << reply << endl;
            send(fd, reply.c_str(), reply.length(), 0);
            close(fd);
            continue;
        }
        
        cout << reply << endl;
        send(fd, reply.c_str(), reply.length(), 0);
        close(fd);
    }
    // 0419 stop server
    pthread_join(stop_server_thread, &r);
    close(sock);
    cout << "Server closed connection" << endl;
    
    void* r2;
    // kill arduino_thread since stopped server
    signal(SIGUSR1, handler);
    pthread_kill( *(pthread_t*)p, SIGUSR1);
    pthread_exit(NULL);
}

void calculate_data(deque<double> temps) {
    double sum = 0;
    deque<double>::iterator it = temps.begin();
    low = *it;
    high = *it;
    sum += *it;
    *it++;
    
    while(it != temps.end()) {
        sum += *it;
        if(*it - high > 0) {
            high = *it;
        }
        if(*it - low < 0) {
            low = *it;
        }
        *it++;
    }
    
    avg = sum / temps.size();
}

void configure(int fd) {
    struct termios pts;
    tcgetattr(fd, &pts);
    cfsetospeed(&pts, 9600);
    cfsetispeed(&pts, 9600);
    
    pts.c_cflag &= ~PARENB;
    pts.c_cflag &= ~CSTOPB;
    pts.c_cflag &= ~CSIZE;
    pts.c_cflag |= CS8;
    pts.c_cflag &= ~CRTSCTS;
    pts.c_cflag |= CLOCAL | CREAD;
    
    pts.c_iflag |= IGNPAR | IGNCR;
    pts.c_iflag &= ~(IXON | IXOFF | IXANY);
    pts.c_lflag |= ICANON;
    pts.c_oflag &= ~OPOST;
    
    tcsetattr(fd, TCSANOW, &pts);
    
    // struct  termios pts;
    // tcgetattr(fd, &pts);
    // cfsetospeed(&pts, 9600);
    // cfsetispeed(&pts, 9600);
    // tcsetattr(fd, TCSANOW, &pts);
}

/*
 * This function gets called when a particular signal is sent to a thread.
 * As you can see, it just ends the function.
 */
void handler(int signum) {
    pthread_exit(NULL);
}

// 0419 stop arduino
void* stop_arduino(void* p){
    sleep(3);
    signal(SIGUSR2, handler);
    // to kill the arduino thread
    pthread_kill( *(pthread_t*)p, SIGUSR2 );
    // update the arduino status
    arduino_status = false;
    return NULL;
}

void* read_arduino(void* p) {
    int counter = 0;
    configure(arduino_fd);
    char buf[100];
    string message;
    int j = 0;
    int bytes_read;
    pthread_t stop_arduino_thread;
    
    while(true){
        message.clear();
        j = 0;
        pthread_create(&stop_arduino_thread, NULL, stop_arduino, p);
        bytes_read = read(arduino_fd, buf, 100);
        while(bytes_read == -1){
            bytes_read = read(arduino_fd, buf, 100);
        }
        
        // kill the stop_arduino thread
        signal(SIGUSR1, handler);
        pthread_kill(stop_arduino_thread, SIGUSR1);
        
        while(buf[j] != '\n' && j < bytes_read){
            message += buf[j];
            j++;
        }
        
        if(message.length() != 0) {
            if(sscanf(message.c_str(), "The temperature is %lf degree C\n", &num) > 0){
                cout << num << endl;
                temp_queue.push_back(num);
            }else{
                continue;
            }
            
            // only keep 3600 records (1 record per second, 3600 records per hour)
            if(temp_queue.size() > 3600){
                temp_queue.pop_front();
            }
        }else{
            continue;
        }
    }
    return 0;
}

// 0419 stop server
void* stop_server(void* p){
    string input;
    cout << "press q & Enter to quit:" << endl;
    while(input != "q"){
        cin >> input;
    }
    
    pthread_mutex_lock(&lock1);
    server_status = false;
    pthread_mutex_unlock(&lock1);
    return NULL;
}

// 0419 stop server
int is_server_open(){
    int result = 1;
    pthread_mutex_lock(&lock1);
    result = server_status;
    pthread_mutex_unlock(&lock1);
    return result;
}
