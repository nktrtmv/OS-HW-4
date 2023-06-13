#include <stdio.h>     
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define MAXPENDING 5
#define GARDEN_X 3
#define GARDEN_Y 3
#define OBSTACLE_COUNT 3
#define MOVE_TIME 1
#define CLEAR 0
#define OBSTACLE -2
#define WORKED -1

pthread_mutex_t mutex;
int garden[GARDEN_X][GARDEN_Y];
int multicast_sock;
struct sockaddr_in multicastAddr;
char logMessage[64];

typedef struct thread_args {
    int socket;
    int number;
} thread_args;

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void *clientThread(void *args) {
    int server_socket;
    struct sockaddr_in client_addr;
    int client_length = sizeof(client_addr);
    pthread_detach(pthread_self());
    server_socket = ((thread_args*)args)->socket;
    int number = ((thread_args*)args)->number;
    free(args);
    int recv_buffer[4];
    int send_buffer[4];
    int x, y;
    for(;;) {
        recvfrom(server_socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*) &client_addr, &client_length);
        if (recv_buffer[0] == -1) {
            printf("Disconected\n");
            break;
        }
        pthread_mutex_lock(&mutex);
        if (recv_buffer[0] == 1) {
            x = recv_buffer[1];
            y = recv_buffer[2];
            if (garden[x][y] > 0) {
                snprintf(logMessage, sizeof(logMessage), "[%d] Waiting for other worker to finish\n", number);
                sendto(multicast_sock, logMessage, sizeof(logMessage), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
                send_buffer[0] = 1;
            } else if (garden[x][y] == CLEAR) {
                snprintf(logMessage, sizeof(logMessage), "[%d] Working on cell [%d][%d]\n", number, x, y);
                sendto(multicast_sock, logMessage, sizeof(logMessage), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
                send_buffer[0] = 2;
                garden[x][y] = number;
            } else {
                snprintf(logMessage, sizeof(logMessage), "[%d] Nothing to do [%d][%d]\n", number, x, y);
                sendto(multicast_sock, logMessage, sizeof(logMessage), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
                send_buffer[0] = 3;
            }
        } else if (recv_buffer[0] == 2) {
            snprintf(logMessage, sizeof(logMessage), "[%d] Finished working on cell [%d][%d]\n", number, x, y);
            sendto(multicast_sock, logMessage, sizeof(logMessage), 0, (struct sockaddr*) &multicastAddr, sizeof(multicastAddr));
            x = recv_buffer[1];
            y = recv_buffer[2];
            garden[x][y] = WORKED;
        }
        pthread_mutex_unlock(&mutex);
        sendto(server_socket, send_buffer, sizeof(send_buffer), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
    }
}

int createSocket(unsigned short server_port) {
    int server_socket;
    struct sockaddr_in server_addr;

    if ((server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("socket() failed");
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) DieWithError("bind() failed");
    printf("Open socket on %s:%d\n", inet_ntoa(server_addr.sin_addr), server_port);
    return server_socket;
}

int main(int argc, char *argv[])
{
    unsigned short client_1_port;
    unsigned short client_2_port;
    int server_1_socket;
    int server_2_socket;
    char *multicastIP;
    unsigned short multicastPort;
    pthread_t thread1;
    pthread_t thread2;
    pthread_mutex_init(&mutex, NULL);
    if (argc != 5)
    {
        fprintf(stderr, "Usage:  %s <Multicast addr> <Multicast port> <Port for 1st client> <Port for 2nd client>\n", argv[0]);
        exit(1);
    }

    multicastIP = argv[1];
    multicastPort = atoi(argv[2]);

    for (int i = 0; i < GARDEN_X; i++) {
        for (int j = 0; j < GARDEN_Y; j++) {
            garden[i][j] = CLEAR;
        }
    }

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        garden[rand() % GARDEN_X][rand() % GARDEN_Y] = OBSTACLE;
    }

    garden[0][0] = 1;
    garden[GARDEN_X - 1][GARDEN_Y - 1] = 2;


    for (int i = 0; i < GARDEN_X; i++) {
        for (int j = 0; j < GARDEN_Y; j++) {
            printf("[%d] ",garden[i][j]);
        }
        printf("\n");
    }

    client_1_port = atoi(argv[3]);
    client_2_port = atoi(argv[4]);

    server_1_socket = createSocket(client_1_port);
    server_2_socket = createSocket(client_2_port);
    if ((multicast_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    int multicastTTL = 1;
    if (setsockopt(multicast_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL, sizeof(multicastTTL)) < 0) 
        DieWithError("setsockopt() failed");
    memset(&multicastAddr, 0, sizeof(multicastAddr));   
    multicastAddr.sin_family = AF_INET;                 
    multicastAddr.sin_addr.s_addr = inet_addr(multicastIP);
    multicastAddr.sin_port = htons(multicastPort); 
    printf("Open multicast socket on %s:%d\n", inet_ntoa(multicastAddr.sin_addr), multicastPort);

    thread_args *args_1 = (thread_args*) malloc(sizeof(thread_args));
    args_1->socket = server_1_socket;
    args_1->number = 1;
    if (pthread_create(&thread1, NULL, clientThread, (void*) args_1) != 0) DieWithError("pthread_create() failed");

    thread_args *args_2 = (thread_args*) malloc(sizeof(thread_args));
    args_2->socket = server_2_socket;
    args_2->number = 2;
    if (pthread_create(&thread1, NULL, clientThread, (void*) args_2) != 0) DieWithError("pthread_create() failed");

    for (;;) {
        for (int i = 0; i < GARDEN_X; i++) {
            for (int j = 0; j < GARDEN_Y; j++) {
                printf("[%d] ",garden[i][j]);
            }
            printf("\n");
        }
        printf("----------\n");
        sleep(1);
    }
    pthread_mutex_destroy(&mutex);
    return 0;
}
