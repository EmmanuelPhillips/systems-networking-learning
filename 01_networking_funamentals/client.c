#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT "3490" // Port clients will connect to
#define BACKLOG 10  // Max pending connections allowed in system queue

// Signal handler to reap dead child processes created by fork()
void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

// Extracts IPv4 or IPv6 address pointer from sockaddr struct
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void) {
  int sockfd,
      new_fd; // sockfd = listening socket; new_fd = active connection socket
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // Store connecting client's address info
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream socket
  hints.ai_flags = AI_PASSIVE;     // Use my local machine's IP automatically

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // Loop through results and bind to the first available socket
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    // SO_REUSEADDR allows port reuse immediately after restart (prevents
    // "Address already in use" errors)
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // Bind socket to local port 3490
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break; // Successfully bound!
  }

  freeaddrinfo(servinfo); // Done with servinfo linked list

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  // Start listening for incoming connections
  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  // Set up sigaction to reap zombie child processes
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  // Main accept loop
  while (1) {
    sin_size = sizeof their_addr;
    // accept() blocks until a client calls connect(), then creates new_fd
    // specifically for that client
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    // fork() creates a child process to handle this specific client connection
    if (!fork()) {
      close(sockfd); // Child process doesn't need the listening socket

      // Send message to client using new_fd
      if (send(new_fd, "Hello, world!", 13, 0) == -1) {
        perror("send");
      }

      close(new_fd); // Close client connection in child
      exit(0);       // Child process finishes and exits
    }

    close(
        new_fd); // Parent process closes new_fd (child process is handling it)
  }

  return 0;
}
