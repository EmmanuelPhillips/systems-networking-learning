// imports needed for stream server
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT "3490" // the port this server will listen on
#define BACKLOG 10 // allow 10 clients to be in the queue waiting to be accepted

void sigchld_handler(int s) {
  (void)s; // unused, silences the -Wextra warning

  int saved_errno = errno; // waitpid might overwrite errno, so we copy it here
                           // to restore later.
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ; // if a client that was forked becomes finished, clean it up.

  errno = saved_errno; // restore the original errno
}

void *
get_in_addr(struct sockaddr *sa) { // picks ipv4 or 6 out of a generic sockaddr.
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void) {
  struct addrinfo hints;     // describes what kind of address we want
  struct addrinfo *servinfo; // head of the linked list of results
  struct addrinfo *p;        // cursor for walking the list

  int sockfd;               // socket file descriptor
  int yes = 1;              // value passed to setsockopt to switch an option on
  int rv;                   // return value of getaddrinfo
  char s[INET6_ADDRSTRLEN]; // buffer for a printable address (debug loop)

  struct sockaddr_storage
      their_addr; // holds client's address (storage can hold both ipv4 and 6)
  socklen_t sin_size; // size of the client's address
  int new_fd;         // socket for talking to the accepted client

  memset(&hints, 0, sizeof hints); // zero out hints
  hints.ai_family = AF_UNSPEC;     // ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM; // tcp
  hints.ai_flags = AI_PASSIVE;     // use my own ip

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // debug loop: print every address getaddrinfo found (no freeing in here)
  for (p = servinfo; p != NULL; p = p->ai_next) {
    void *addr;
    if (p->ai_family == AF_INET) {
      addr = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
    } else {
      addr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
    }
    inet_ntop(p->ai_family, addr, s, sizeof s);
    printf("found: family %d -> %s\n", p->ai_family, s);
  }

  // loop through results, bind to the first one we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue; // try the next result
    }

    // let us reuse the port straight after restarting the server
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue; // try the next result
    }

    break; // socket created and bound, stop looking
  }

  // the list is only freed once, after both loops are finished with it
  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  printf("server: bound to port %s\n", PORT);

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen"); // if listening throws an error then exit
    exit(1);
  }

  struct sigaction sa; // create struct to handle complete child clients
  sa.sa_handler = sigchld_handler; // run this function when a child exits
  sigemptyset(&sa.sa_mask);
  sa.sa_flags =
      SA_RESTART; // restart calles that were interrupted (e.g. if an accept was
                  // blocked from starting, rather than failing)
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  while (1) {
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) { // if the client cant be accepted
      perror("accept");
      continue; // a failed accept doesn't kill the server, it shoudl attempt to
                // accept a new one
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s); // convert client address to printable text
    printf("server: got connection from %s\n", s);

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      close(new_fd);
      continue; // if the fork fails then drop the client and try to serve
                // another one
    }
    if (pid == 0) {
      close(sockfd); // child doesn't need to use the listening socket
      if (send(new_fd, "hello, world!\n", 14, 0) == -1) {
        perror("send");
      }
      close(new_fd);
      exit(0); // the child will end here
    }
    close(new_fd);
  }

  return 0;
}
