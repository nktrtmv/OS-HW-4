#include <stdio.h>   
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GARDEN_X 3
#define GARDEN_Y 3
#define OBSTACLE_COUNT 6
#define MOVE_TIME 1
#define CLEAR 0
#define OBSTACLE -2
#define WORKED -1

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[])
{
    int client_socket;
    int work_time;
    int index;
    struct sockaddr_in server_addr;
    unsigned short server_port;
    int recv_msg_size;
    char *server_ip;

    if (argc != 5)
    {
       fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Worker index> <work time>\n", argv[0]);
       exit(1);
    }

    server_ip = argv[1];
    server_port = atoi(argv[2]);
    index = atoi(argv[3]);
    work_time = atoi(argv[4]);

    if ((client_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("socket() failed");

    memset(&server_addr, 0, sizeof(server_addr));  
    server_addr.sin_family      = AF_INET;        
    server_addr.sin_addr.s_addr = inet_addr(server_ip); 
    server_addr.sin_port        = htons(server_port);

    int server_len = sizeof(server_addr);

    printf("I am worker #%d\n", index);
    int recv_buffer[4];
    int send_buffer[4];
    int x, y;
    int dir = 1;
    if (index == 1) {
        x = 0;
        y = 0;
    } else {
        x = GARDEN_X - 1;
        y = GARDEN_Y - 1;
    }
    send_buffer[0] = 2;
    send_buffer[1] = x;
    send_buffer[2] = y;
    sendto(client_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
    recvfrom(client_socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &server_addr, &server_len);
    for (;;) {
        if (index == 1) {
            y += dir;
            if (dir == 1 && y >= GARDEN_Y) {
                y -= dir;
                x++;
                dir *= -1;
            }
            if (dir == -1 && y < 0) {
                y -= dir;
                x++;
                dir *= -1;
            }
            if (x >= GARDEN_X) {
                printf("[%d] Finished\n", index);
                break;
            }
        } else {
            x -= dir;
            if (dir == 1 && x < 0) {
                y--;
                x += dir;
                dir *= -1;
            }
            if (dir == -1 && x >= GARDEN_X) {
                y--;
                x += dir;
                dir *= -1;
            }
            if (y < 0) {
                printf("[%d] Finished\n", index);
                break;
            }
        }
        printf("[%d] Going to [%d][%d]\n", index, x, y);
        send_buffer[0] = 1;
        send_buffer[1] = x;
        send_buffer[2] = y;
        sendto(client_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
        recvfrom(client_socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &server_addr, &server_len);
        if (recv_buffer[0] == 1) {
            printf("[%d] Waiting for other worker to finish\n", index);
            do {
                sleep(1);
                sendto(client_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
                recvfrom(client_socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &server_addr, &server_len);
            } while (recv_buffer[0] == 1);
           
        }
        if (recv_buffer[0] == 2) {
            printf("[%d] Working on cell [%d][%d]\n", index, x, y);
            sleep(work_time);
            printf("[%d] Finished working on cell [%d][%d]\n", index, x, y);
            send_buffer[0] = 2;
            sendto(client_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
            recvfrom(client_socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &server_addr, &server_len);
        } else {
            printf("[%d] Nothing to do [%d][%d]\n", index, x, y);
            sleep(MOVE_TIME);
        }
    }
    send_buffer[0] = -1;
    sendto(client_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
    close(client_socket);
    return 0;
}
