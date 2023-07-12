#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"


int cli_sd = -1;

static bool nread(int fd, int len, uint8_t *buf)
{
  int x = 0;
  while(x < len){
    int n = read(fd, &buf[x], len - x);
    if(n < 0){
      return false;
    }
    x += n;
  }
  return true;
}


static bool nwrite(int fd, int len, uint8_t *buf)
{
  int num_write = 0; 
  while(num_write < len){
    int x = write(fd, &buf[num_write], len - num_write);

    if(x < 0){
      return false;
    }
    num_write += x;
  }

  return true;
}

static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block){

  uint16_t len;
  uint8_t head[HEADER_LEN];
  
  nread(fd, HEADER_LEN, head);

  memcpy(&len, head, 2);
  memcpy(op, head + 2, 4);
  memcpy(ret, head + 6, 2);

  *op = ntohl(*op);
  len = ntohs(len);
  *ret = ntohs(*ret);
 

  if(len == 264){
    nread(fd, 256, block);
  }
  else{
    return false;
  }

  return true;
}

static bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint16_t len = HEADER_LEN;
  uint32_t command = op >> 26;

  op = htonl(op);

  if(command != JBOD_WRITE_BLOCK){
    uint8_t packet[HEADER_LEN];
    len = htons(len);
    memcpy(packet, &len, 2);
    memcpy(packet + 2, &op, 4);
    nwrite(sd, 8, packet);
  }

  else{
    uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE];
    len += JBOD_BLOCK_SIZE;
    len = htons(len);
    memcpy(packet, &len, 2);
    memcpy(packet + 2, &op, 4);
    memcpy(packet + 8, block, 256);
    nwrite(sd, 264, packet);
  }
 
  return true;
}

bool jbod_connect(const char *ip, uint16_t port)
{
  struct sockaddr_in caddr;
  caddr.sin_port = htons(JBOD_PORT);
  caddr.sin_family = AF_INET;

  if(inet_aton(ip, &(caddr.sin_addr)) == 0)
  {
    return false;
  }

  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1)
  {
    printf("Error on socket creation [%s]\n", strerror(errno));
    return false;
  }
  if (connect(cli_sd, (struct sockaddr *)&caddr, sizeof(caddr)) == -1)
  {
    return false;
  }

  return true;
}

void jbod_disconnect(void)
{
  close(cli_sd);
  cli_sd = -1;
}


int jbod_client_operation(uint32_t op, uint8_t *block)
{
  uint16_t ret;
  uint32_t rop;
  if(send_packet(cli_sd, op, block))
  {
    recv_packet (cli_sd, &rop, &ret, block);  
  }
  return ret;
}