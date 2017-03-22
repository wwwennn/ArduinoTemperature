#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <iostream>
using namespace std;


/*
 This code configures the file descriptor for use as a serial port.
 */
void configure(int fd) {
    struct  termios pts;
    tcgetattr(fd, &pts);
    cfsetospeed(&pts, 9600);
    cfsetispeed(&pts, 9600);
    tcsetattr(fd, TCSANOW, &pts);
}

string message;

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        cout << "Please specify the name of the serial port (USB) device file!" << endl;
        exit(0);
    }
    
    // get the name from the command line
    char* file_name = argv[1];
    
    // try to open the file for reading and writing
    int fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (fd < 0) {
        perror("Could not open file");
        exit(1);
    }
    else {
        cout << "Successfully opened " << argv[1] << " for reading/writing" << endl;
    }
    
    configure(fd);
    
    
    char buf[100];
    int start = 0;
    int bytes_read = read(fd, buf, 100);
    int end = bytes_read;
    
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
            cout << message << endl;
            message.clear();
            start = ++j;
        } else {
            bytes_read = read(fd, buf, 100);
            start = 0;
            end = bytes_read;
        }
    }
    
    /*
     Write the rest of the program below, using the read and write system calls.
     */
    
}
