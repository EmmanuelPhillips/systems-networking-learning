// imports needed for stream client
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "3490"     // the port the server is listening on
#define MAXDATASIZE 100 // max number of bytes we can receive at once

// picks the ipv4 or ipv6 address out of a generic sockaddr and returns a
// pointer to it
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)
                 ->sin_addr); // pointer to the 4 byte ipv4 address
  }
  return &(((struct sockaddr_in6 *)sa)
               ->sin6_addr); // pointer to the 16 byte ipv6 address
}

int main(int argc, char *argv[]) {
  struct addrinfo hints;     // describes what kind of address we want
  struct addrinfo *servinfo; // head of the linked list of results
  struct addrinfo *p;        // cursor for walking the list

  int sockfd;               // socket file descriptor
  int numbytes;             // how many bytes recv actually received
  int rv;                   // return value of getaddrinfo
  char buf[MAXDATASIZE];    // buffer the received message is written into
  char s[INET6_ADDRSTRLEN]; // buffer for a printable address

  if (argc !=
      2) { // argv[0] is the program name, argv[1] is the host to connect to
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints); // zero out hints
  hints.ai_family = AF_UNSPEC;     // ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM; // tcp
  // no AI_PASSIVE: we want the address of the host we name, not our own

  if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // debug loop: print every address found for the host (no freeing in here)
  for (p = servinfo; p != NULL; p = p->ai_next) {
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, sizeof s);
    printf("found: family %d -> %s\n", p->ai_family, s);
  }

  // loop through results, connect to the first one we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue; // try the next result
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue; // try the next result
    }

    break; // socket created and connected, stop looking
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  // must come before freeaddrinfo, because p points into the list
  inet_ntop(p->ai_family, get_in_addr(p->ai_addr), s, sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // done with the list, only free it once

  // receive the server's message (leave one byte spare for the null terminator)
  if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    perror("recv");
    exit(1);
  }

  buf[numbytes] =
      '\0'; // recv does not null terminate, so we do it to print as a string

  printf("client: received '%s'\n", buf);

  close(sockfd);

  return 0;
}
