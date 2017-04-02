#include <string>
#include <iostream>

using namespace std;

void configure(int fd) {
    struct  termios pts;
    tcgetattr(fd, &pts);
    cfsetospeed(&pts, 9600);
    cfsetispeed(&pts, 9600);
    tcsetattr(fd, TCSANOW, &pts);
}

//string get_message(int fd) {
//    configure(fd);
//    char buf[100];
//    int start = 0;
//    int bytes_read = read(fd, buf, 100);
//    int end = bytes_read;
//    string message;
//    
//    while(true) {
//        int j = start;
//        
//        while(j < end) {
//            if(buf[j] == '\n') {
//                break;
//            }
//            message += buf[j];
//            j++;
//        }
//        
//        if(j < end) {
//            cout << message << endl;
//            message.clear();
//            start = ++j;
//        } else {
//            bytes_read = read(fd, buf, 100);
//            start = 0;
//            end = bytes_read;
//        }
//    }
//    
//    return message;
//}
