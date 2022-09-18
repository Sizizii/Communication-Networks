/*
** client.c -- a stream socket client demo
*/

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

#define PORT "3490" // the port client will be connecting to
#define HTTP_PORT "80"

#define MAXDATASIZE 1024 // max number of bytes we can get at once
#define MAXHOSTNAMESIZE 128
#define MAXPORTSIZE 6
#define MAXPATHSIZE 128

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  char recvbuf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  // hints will hold the socket address information
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2)
  {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  /*************** Extract information from argv ********************/
  char hostname[MAXHOSTNAMESIZE];
  char client_port[MAXPORTSIZE];
  char path2file[MAXPATHSIZE];
  /* empty three allocated strings, avoiding garbage */
  // memset(hostname, 0, MAXHOSTNAMESIZE);
  // memset(client_port, 0, MAXPORTSIZE);
  // memset(path2file, 0, MAXPORTSIZE);

  memset(&hints, 0, sizeof hints); // empty struct
  hints.ai_family = AF_UNSPEC;     // getaddrinfo takes care of variable ai_family
  hints.ai_socktype = SOCK_STREAM; // since using TCP only, use SOCK_STREAM for the socket type --> see the slide
  int arg_temp_idx;
  int arg_len = strlen(argv[1]);

  // for the domain name
  if (argv[1][0] == 'h')
  {
    arg_temp_idx = 7; // leave out "http://"
  }
  else
  {
    arg_temp_idx = 0; // for those starting from "www."
  }

  // for the host name
  int has_port = 0;
  int hostname_idx = 0;
  for (; arg_temp_idx < arg_len; arg_temp_idx++)
  {
    if (argv[1][arg_temp_idx] == ':')
    {
      /* port starts with : */
      has_port = 1;
      arg_temp_idx++;
      break;
    }
    else if (argv[1][arg_temp_idx] == '/')
    {
      /* default port */
      break;
    }
    else
    {
      hostname[hostname_idx] = argv[1][arg_temp_idx];
      hostname_idx++;
    }
  }

  // for the port
  if (has_port == 1)
  {
    int port_idx = 0;
    for (; arg_temp_idx < arg_len; arg_temp_idx++)
    {
      if (argv[1][arg_temp_idx] == '/')
      {
        break;
      }
      else
      {
        client_port[port_idx++] = argv[1][arg_temp_idx];
      }
    }
  }
  else
  {
    strcpy(client_port, HTTP_PORT);
  }

  // for the path to file
  int path_len = strlen(argv[1]) - arg_temp_idx;
  strncpy(path2file, argv[1] + arg_temp_idx, path_len);

  printf("Host: %s, PORT: %s, PATH: %s\n", hostname, client_port, path2file);
  // printf("\n");

  /************** Finish processing input args **********************/

  /*            getaddrinfo
   * 1st arg: server name passed from the terminal
   * 2nd arg: PORT to be connected to
   * 3rd arg: hints that we've defined to hold the addr info
   *  returns a linked list
   */
  if ((rv = getaddrinfo(hostname, client_port, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1)
    {
      perror("client: socket");
      continue;
    }
    // connect to a socket
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL)
  {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  // parsing the addr info to convert it to a string
  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure

  /* Once connnection is built, send the header */
  sprintf(buf, "GET %s HTTP/1.1\r\n", path2file);
  strcat(buf, "User-Agent: Wget/1.12 (linux-gnu)\r\n");
  strcat(buf, "Host: ");
  strcat(buf, hostname);
  strcat(buf, ":");
  strcat(buf, client_port);
  strcat(buf, "\r\n");
  strcat(buf, "Connection: Keep-Alive\r\n\r\n");
  printf("%s", buf);

  if (send(sockfd, buf, strlen(buf), 0) == -1)
  {
    printf("Error sending.\n");
    perror("send");
    exit(1);
  }

  FILE *fptw;
  fptw = fopen("output", "w");
  if (!fptw)
  {
    perror("open");
    close(sockfd);
    exit(1);
  }
  int check_recv = 0;
  printf("Start receiving file.\n");
  while (1)
  {
    if ((numbytes = recv(sockfd, recvbuf, MAXDATASIZE - 1, 0)) > 0)
    {
      if (check_recv == 1)
      {
        printf("Continue writing to output.\n");
        recvbuf[numbytes] = '\0';
        fwrite(recvbuf, 1, numbytes, fptw);
      }
      else
      {
        if (strncmp(recvbuf + strlen("HTTP/1.1 "), "200 OK", 6) == 0)
        {
          check_recv = 1;
          printf("Read the file successfully.\n");
          char *file_start_from_ = strstr(recvbuf, "\r\n\r\n");
          // printf("%s\n", recvbuf);
          // printf("%s\n", file_start_from_ + strlen("\r\n\r\n"));
          fwrite(file_start_from_ + strlen("\r\n\r\n"), 1, numbytes - (file_start_from_ - recvbuf) - strlen("\r\n\r\n"), fptw);
        }
        else
        {
          printf("Read failed.");
          // fprintf(stderr, "Fail to read such file from path.");
          perror("read file");
          break;
        }
      }
    }
    else
    {
      break;
    }
  }

  fclose(fptw);
  close(sockfd);

  return 0;
}
