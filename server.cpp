#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "8080"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    // Como es una signal, esta dentro de un scope donde el value errno existe
    int saved_errno = errno;
    // This part wait for every child process to finish, when all the process finish return 0 else return
    // id of the child process
    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p; // Use this struct to save information about ip
    // The addinfo hint is use with getaddrinfo to specify that the connection could be random, and get a link
    // list of all possible connection and create a process to handle every connection

    //defines a variable that will be used to hold the address information of the client that connects to the server.
    struct sockaddr_storage their_addr; // connector's address information

    socklen_t sin_size;
    
    /* sigation:
     *defines a variable that will be used to specify the action that should be taken when a SIGCHLD 
     signal is receive
     * */
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    //char buffer[1024];
    // initailize hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // unspecify
    hints.ai_socktype = SOCK_STREAM; // tcp
    hints.ai_flags = AI_PASSIVE; // use my IP

    /*
     * getaddinfo: receive first parameter null mean to use ip address from the server
     * int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP
                const char *service,  // e.g. "http" or port number
                const struct addrinfo *hints,
                struct addrinfo **res);
    You give this function three input parameters, and it gives you a pointer to a linked-list, res, of results.

The node parameter is the host name to connect to, or an IP address.
     * function to get a list of potential addresses that the server can bind to, 
     * based on the specified criteria. If the function fails, an error message is printed and 
     * the program exits with an error code.
     * */
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // Servinfo is a link list that was populate by getaddrinfo
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // create a socket file descriptor for every ip of the server
        // int socket(int domain, int type, int protocol); (domain is ipv4 or ipv6, type if is tcp or ucd
        // protocol could be set to 0
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        /*setsockopt: Sets the SO_REUSEADDR socket option for the socket referred to by the sockfd
         * file descriptor, allowing reuse of local addresses. If an error occurs, 
         * an error message is printed and the program exits with an error code.
         *
         *At socket level SOL_SOCKET, to set reuseador when a server restart access the port without 
         waiting to be free
         * */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        // BInd the socketfd to your address and port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure, always free after call getaddrinfo

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    // Listen for connection in this socket, backlog nro of connections
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    /*
     * set up a signal handler for the SIGCHLD signal using the sigaction() function. When a child process 
     * terminates, the sigchld_handler() function will be called to reap any dead child processes.
     *
     * */
    // Set the sa signal handlers
    sa.sa_handler = sigchld_handler; // reap all dead processes, function define above
    sigemptyset(&sa.sa_mask);// initializer the sa_mask and block other process until the function is terminate
    sa.sa_flags = SA_RESTART;// If a system is interrupted restart the signal
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");// THis function set the signal handler defines in the sa to the SIGCHLD signal
        exit(1);
    }


    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        // When we accept coneection from client create a new socekt to save information about this connection
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) { // Information about client address is save in their_adrr
            perror("accept");
            continue;
        }
        // convert the binary address to a redable string in human form and print it
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        /*
         *creates a new process to handle each incoming client connection. In the child process, 
         a message is sent to the client before closing the connection and exiting. 
         In the parent process, the connected socket is closed and the server continues to listen 
         for new connections.
         * */
        //memset(buffer, 0, sizeof(buffer));
        if (!fork()) { // this is the child process, mean if fork return 0 (success)
            close(sockfd); // child doesn't need the listener, child dont need to listen for connection
            if (send(new_fd, "Hello, world!", 13, 0) == -1)// send messages also we could read messages here
                perror("send");
            char* buffer[1024];
            while ( (numbytes = recv(new_fd,buffer, 1023, 0)) > 0) {
                printf("server received '%s'\n", buffer);
                memset(buffer, 0, sizeof(buffer));
            }
            if (numbytes == -1) 
                perror("error received");
            //buffer[numbytes] = '\n';
            //printf("server received '%s'\n", buffer);
            close(new_fd);// close the connection after send the messages
            exit(0); // exit the process with 0, if something happen the signal above is executed
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}
