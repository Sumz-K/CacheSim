#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <string.h>
#include <ctype.h>

#define NUMTHREADS 2
#define MEMORYSIZE 24
#define CACHESIZE 5

typedef char byte;

/*
STATE
0-> INVALID
1->SHARED
2->EXCLUSIVE
3->MODIFIED


*/
byte *memory;

char *file_arr[] = {"input_0.txt", "input_1.txt"};
struct cache
{
    byte address; // This is the address in memory.
    byte value;   // This is the value stored in cached memory.
    // State for you to implement MESI protocol.
    int state;
};
typedef struct cache cache;
cache *cache_arr[NUMTHREADS];
struct decoded_inst
{
    int type; // 0 is RD, 1 is WR
    byte address;
    byte value; // Only used for WR
};

struct bus_transaction
{
    int type; // 0 RD 1 WR
    byte address;
    byte value;
    int thread_id;
};

typedef struct decoded_inst decoded;
typedef struct bus_transaction transaction;

/*
 * This is a very basic C cache simulator.
 * The input files for each "Core" must be named core_1.txt, core_2.txt, core_3.txt ... core_n.txt
 * Input files consist of the following instructions:
 * - RD <address>
 * - WR <address> <val>
 */

// Decode instruction lines
decoded decode_inst_line(char *buffer)
{

    // WR 20 10
    // RD 20
    decoded inst;
    char inst_type[2];
    sscanf(buffer, "%s", inst_type);
    if (!strcmp(inst_type, "RD"))
    {
        inst.type = 0;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    }
    else if (!strcmp(inst_type, "WR"))
    {
        inst.type = 1;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;
    }
    return inst;
}


void print_cachelines(cache *c, int cache_size)
{
    for (int i = 0; i < cache_size; i++)
    {
        cache cacheline = *(c + i);
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}

void print_bus_transaction(transaction *t)
{
    if (t->type == 1)
    {
        printf("WR %d %d by thread %d\n", t->address, t->value, t->thread_id);
    }
    else
    {
        printf("RD %d by thread %d\n", t->address, t->thread_id);
    }
}

void update_current_cache_state(cache *c, int old_state, int inst_type)
{
    switch (old_state)
    {
    case 0:
        if(inst_type==0){
            //c->state=1;
            c->state=0;
        }
        else{
            c->state=3;
        }
        break;
    case 1:
        if (inst_type == 0)
        { // read

            c->state = 1;
        }
        else
        {
            c->state = 3;
        }
        break;
    case 2:
        if (inst_type == 0)
        {
            c->state = 2;
        }
        else
        {
            c->state = 3;
        }
        break;
    case 3:
        c->state = 3;
        break;
    default:
        break;
    }
}

void snoop(int issue_thread_id, int inst_type, transaction *t)
{
    // printf("Snooping\n");
    for (int i = 0; i < NUMTHREADS ; i++)
    {
        if (issue_thread_id == i)
        {
            continue;
        }
        else
        {
            cache *c = cache_arr[i];
            for (int j = 0; j < CACHESIZE; j++)
            {
                if (c[j].address == t->address)
                {

                    switch (inst_type)
                    {
                    case 0:
                        if (c[j].state == 1 || c[j].state == 2 || c[j].state == 3)
                        {
                            c[j].state = 1;
                        }
                        break;
                    case 1:
                        // if (c[j].state != 3)
                        // {
                        //     c[j].state = 0;
                        // }

                        c[j].state=0;
                        break;
                    }
                }
            }
            cache_arr[i] = c;
        }
    }
}
// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads)
{

#pragma omp parallel num_threads(num_threads)  shared(cache_arr, memory)
    {
        int cache_size = CACHESIZE;
        
        int thread_id = omp_get_thread_num();
        cache *c = (cache *)malloc(sizeof(cache) * cache_size);

        cache_arr[thread_id] = (cache *)malloc(sizeof(cache) * cache_size);


        memcpy(cache_arr[thread_id], c, sizeof(cache) * cache_size);

        char *filename = file_arr[thread_id];
        FILE *inst_file = fopen(filename, "r");
        char inst_line[20];

        while (fgets(inst_line, sizeof(inst_line), inst_file))
        {
            decoded inst = decode_inst_line(inst_line);

            int hash = inst.address % cache_size;
            cache cacheline = *(c + hash);

            transaction *t = (transaction *)malloc(sizeof(transaction));
            t->type = inst.type;
            t->address = inst.address;
            t->value = inst.value;
            t->thread_id = thread_id;

            if (cacheline.address == inst.address && cacheline.state!=0)
            { // Cache hit
               // printf("Thread %d: Cache hit for address %d\n", thread_id, inst.address);
                //printf("thread id %d address %d\n",thread_id,cacheline.address);
                snoop(t->thread_id, inst.type, t);
                update_current_cache_state(&cacheline, cacheline.state, inst.type);
                c[hash] = cacheline;
                cache_arr[thread_id] = c;
                
            }
            else
            { // Cache miss
                //printf("Thread %d: Cache miss for address %d\n", thread_id, inst.address);
                int found_modified = 0;
                // if other cache has the block in modified state
                for (int i = 0; i < NUMTHREADS; i++)
                {
                    //printf("hello\n");
                    if (i != thread_id)
                    {
                        
                        cache *other_cache;
                        if(cache_arr[i]!=NULL){
                            other_cache = cache_arr[i];
                        }
                        else{
                            cache_arr[i] = (cache *)malloc(sizeof(cache) * cache_size);
                            for(int k=0;k<cache_size;k++){
                                cache_arr[i][k].address = -1; 
                                cache_arr[i][k].value = 0;    
                                cache_arr[i][k].state = 0;    
                            }
                        }

                        other_cache=cache_arr[i];


                        //printf("hello2\n");
                        for (int j = 0; j < cache_size; j++)
                        {
                            //printf("hello3\n");
                            //printf("Im here %d %d\n", other_cache[j].address, other_cache[j].state);

                            if (other_cache[j].address == inst.address && other_cache[j].state == 3 && !found_modified)
                            {
                                //printf("Found in thread %d", i);

                                found_modified = 1;
                                

                                memory[cacheline.address]=other_cache[j].value;
                                cacheline.value = other_cache[j].value;

                                other_cache[j].state = 1;
                                if (found_modified)
                                {
                                    c[hash] = cacheline;
                                    cache_arr[thread_id] = c;
                                    cache_arr[i]=other_cache;
                                    break;
                                }
                            }
                        }
                    }
                }

                //printf("finished for\n");
                if (!found_modified)
                {
                    //fetch from memory
                    *(memory + cacheline.address) = cacheline.value;
                    cacheline.value = *(memory + inst.address);
                    
                }
                
                cacheline.address = inst.address;
                cacheline.state = 0;
                if (inst.type == 1)
                {
                    cacheline.value = inst.value;
                }
                *(c + hash) = cacheline;
                snoop(t->thread_id, inst.type, t);
                update_current_cache_state(&cacheline, cacheline.state, inst.type);
                //printf("wrote value %d",cacheline.value);
                c[hash] = cacheline;
                cache_arr[thread_id] = c;
                
                
            }

            switch (inst.type)
            {
            case 0:
                printf("Thread %d: Reading from address %d: %d\n", thread_id, cacheline.address, cacheline.value);
                break;

            case 1:
                printf("Thread %d: Writing to address %d: %d\n", thread_id, cacheline.address, cacheline.value);
                break;
            }
        }
        fclose(inst_file);
        free(c);
    }
}

int main(int c, char *argv[])
{
    // Initialize Global memory
    // Let's assume the memory module holds about 24 bytes of data.

    memory = (byte *)malloc(sizeof(byte) * MEMORYSIZE);
    cpu_loop(NUMTHREADS);
    free(memory);
}
