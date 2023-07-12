#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "cache.h"
#include "mdadm.h"
#include "jbod.h"
#include "net.h"

uint32_t encode_op(int cmd, int disk_num, int bloc_num){
  uint32_t op =0;
  op |= (cmd<<26);
  
  uint32_t disk_ID = 0;
  disk_ID |= (disk_num<<22);
  op |= disk_ID; //bit adding opertion part w Disk_ID
  
  uint32_t block_ID = 0;
  block_ID |= bloc_num;
  op |=block_ID; //bit adding ^^ with block_ID

  return op;
}

int min(int a, int b){
  if (a<b){
    return a;
  }
  else{
    return b;
  }
}

int mounted = 0;

int mdadm_mount(void) {
  if (mounted==1){
    return -1;
  }


  uint32_t op = encode_op(JBOD_MOUNT, 0, 0); // Creates Instruction for mounting specifically for jbod_client_operation 


  jbod_client_operation(op, NULL); // can't call jbod twice
  

  mounted=1;
  return 1;
  // int x =1;
}

int mdadm_unmount(void) {
  if (mounted == 0){ //checks to see if it is already mounted
    return -1;
  }
  uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  mounted = 0;
  return 1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {

  if (mounted == 0){ //checks if not mounted
    return -1;
  }


  if (buf==NULL && len>0){// read should fail when passed a NULL pointer and non-zero length but it did not
    return -1;
  }


  if (len>1024){ //read should fail on larger than 1024-byte I/O sizes but it did not
    return -1;
  }


  if (len==0 && buf!=NULL){//failed: 0-length read should succeed with a NULL pointer but it did not
    return -1;
  }

  if (JBOD_NUM_DISKS*JBOD_DISK_SIZE <addr+len){ //makes sure you don't try to access data that is not accessible
    return -1;
  }
 
  int num_read = 0;

  uint8_t mybuf[256];

  // uint32_t disk_id = addr/JBOD_DISK_SIZE; //Finding the disk_id from pos
  // uint32_t block_id = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE; //Finding the block_id from pos
  // //check if cache contains the specific block being read
  // if (cache_lookup(disk_id, block_id, buf) == 1) {
    
  

  while(len > 0)
  {
    uint32_t disk_id = addr/JBOD_DISK_SIZE; //Finding the disk_id from pos
    uint32_t block_id = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE; //Finding the block_id from pos
    uint8_t offset = addr % JBOD_BLOCK_SIZE;

    uint32_t disk = encode_op(JBOD_SEEK_TO_DISK, disk_id, 0);
    jbod_client_operation(disk, NULL);

    uint32_t block = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_id);
    jbod_client_operation(block, NULL);


    if (cache_lookup(disk_id, block_id, mybuf) != 1)
    {
    uint32_t read;
      read = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_client_operation(read, mybuf);
      if (cache_insert(disk_id, block_id, mybuf) != 1){
        cache_update(disk_id, block_id, mybuf);
      }
    }
    int num_bytes_to_read_from_block = min(len, min(256, 256 - offset));

    memcpy(buf + num_read, mybuf + offset, num_bytes_to_read_from_block);

    num_read += num_bytes_to_read_from_block;
    len -= num_bytes_to_read_from_block;
    addr += num_bytes_to_read_from_block;
  }  
  return num_read;
}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {

  if (mounted == 0){ //checks if not mounted
    return -1;
  }


  if (buf==NULL && len>0){// read should fail when passed a NULL pointer and non-zero length but it did not
    return -1;
  }


  if (len>1024){ //read should fail on larger than 1024-byte I/O sizes but it did not
    return -1;
  }


  if (len==0 && buf!=NULL){//failed: 0-length read should succeed with a NULL pointer but it did not
    return -1;
  }

  if (JBOD_NUM_DISKS*JBOD_DISK_SIZE <addr+len){ //makes sure you don't try to access data that is not accessible
    return -1;
  }
 
  int num_write = 0;

  uint8_t mybuf[256];

  while(len > 0)
  {
    uint32_t disk_id = addr/JBOD_DISK_SIZE; //Finding the disk_id from pos
    uint32_t block_id = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE; //Finding the block_id from pos
    uint8_t offset = addr % JBOD_BLOCK_SIZE;

    uint32_t disk = encode_op(JBOD_SEEK_TO_DISK, disk_id, 0);
    jbod_client_operation(disk, NULL);

    uint32_t block = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_id);
    jbod_client_operation(block, NULL);

    uint32_t read = encode_op(JBOD_READ_BLOCK, 0, 0);
    jbod_client_operation(read, mybuf);

    int num_bytes_to_write_from_block = min(len, min(256, 256 - offset));

    memcpy(mybuf + offset, buf + num_write, num_bytes_to_write_from_block);

    uint32_t disk_1 = encode_op(JBOD_SEEK_TO_DISK, disk_id, 0);
    jbod_client_operation(disk_1, NULL);

    uint32_t block_1 = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_id);
    jbod_client_operation(block_1, NULL);

    uint32_t write = encode_op(JBOD_WRITE_BLOCK, 0, 0);
    jbod_client_operation(write, mybuf);

    if (cache_enabled() == true && cache_insert(disk_id, block_id, mybuf) == -1)
        cache_update(disk_id, block_id, mybuf);

    num_write += num_bytes_to_write_from_block;
    len -= num_bytes_to_write_from_block;
    addr += num_bytes_to_write_from_block;
  }  
  return num_write;
  }