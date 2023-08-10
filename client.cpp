/*
** client.c -- a stream socket client demo
*/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "8080" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6: convert a bytes address to a human redable address
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char* msg = "hello say client miquel2";
    /*if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }*/
    // initializer hint to a family whatever, tcp connection
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    //auto host = argv[1];
    
    // get address infor and save it in hint and a link list of servinfo
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) { // aqui iba argv[1] como ip address
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }
    // no connections
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    // convert address bytes to human readable
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    // check if we received a message from the server
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);
    
    printf("client sending message to server now\n");
    if (send(sockfd, msg, strlen(msg), 0) == -1) {
        perror("error sending message");
        exit(1);
    }
    
     // Para usar esto el server debe tener varios read
    std::cout << "Ingrese un numero [1/2] para enviar un mensaje distinto" << std::endl;
    char * msg1 = "Hola como estas";
    char* msg2 = "pan es un gato meon";
    for (int i = 0; i < 2; i++) {
        int aux;
        std::cin >> aux;
        if (aux == 1) {
            if (send(sockfd, msg1, strlen(msg1), 0) == -1){
                perror("Error msg1");
                exit(1);
            }
        }
        if (aux == 2) {
            if (send(sockfd, msg2, strlen(msg2), 0) == -1) {
                perror("Error msg2");
                exit(1);
            }
        }
    }

    close(sockfd);
    return 0;
}
