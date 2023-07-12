#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int num_queries = 0;
static int cache_size = 0;
static int num_hits = 0;
static int clock = 0;

int c = 0;

int cache_create(int num_entries) {//
if((c == 1) || (num_entries < 2) || (num_entries > 4096)){ //returns -1 for failed cases
    return -1;
}

cache = calloc(num_entries, sizeof(cache_entry_t));
cache_size = num_entries;
c = 1;

return 1;
}


int cache_destroy(void) {//
if (c == 0){ 
    return -1;
}

cache_size = 0;
c = 0;
free(cache);
cache = NULL;

return 1;
}


int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
num_queries += 1;
if(cache != NULL){
if((buf == NULL) || (block_num > 256) || (block_num < 0) || (disk_num < 0) || (disk_num > 16)){ //failed cases
    return -1;
}
}

for(int i = 0; i < cache_size; i++){
    if((cache[i].valid == true) && (cache[i].disk_num == disk_num) && (cache[i].block_num == block_num)){ //checks if disk_num and block_num are the same
        memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE); //destination, source, size
        num_hits += 1;
        clock += 1;
        cache[i].access_time = clock;
        return 1;
        }
}

return -1;
}


void cache_update(int disk_num, int block_num, const uint8_t *buf) {

num_queries += 1;

if((c == 1) && (buf != NULL) && (cache_size != 0)){
    for(int i = 0; i < cache_size; i++){
    if((cache[i].valid == true) && (cache[i].disk_num == disk_num) && (cache[i].block_num == block_num)){
        memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
        num_hits += 1;
        clock += 1;
        cache[i].access_time = clock;
        }
    }
}

}


int cache_insert(int disk_num, int block_num, const uint8_t *buf){
num_queries++;

if((cache == NULL) || (buf == NULL) || (block_num < 0) || (block_num > 256) || (disk_num < 0) || (disk_num > 16)){
return -1;
}

for(int i = 0; i < cache_size; i++){
    if (cache[i].valid == true && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
    return -1;

    if(cache[i].valid == false){
        cache[i].disk_num = disk_num;
        cache[i].block_num = block_num;
        clock += 1;
        cache[i].access_time = clock;
        memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
        cache[i].valid = true;
        return 1;
    }
    }
}


int min = 0;
for(int i = 0; i < cache_size; i++){
    if(cache[i].access_time < cache[min].access_time){
        min = i;
    }
}
clock += 1;
cache[min].disk_num = disk_num;
cache[min].block_num = block_num;
cache[min].access_time = clock;
memcpy(cache[min].block, buf, JBOD_BLOCK_SIZE);
return 1;
}



bool cache_enabled(void) {
if(c == 1 && cache != NULL){
return true;
}
return false;
}


void cache_print_hit_rate(void) {
fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}