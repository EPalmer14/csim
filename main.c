//
//  main.c
//  csim
//
//  Created by Zeke Palmer and Siobhan Lounsbury on 5/25/21.
//

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

// Memory address
typedef unsigned long long int MemAddrT;

// Struct for each cache line
typedef struct {
    int valid; // Valid bit
    MemAddrT tag; // Tag
    int timestamp; // Timestamp to keep track of LRU
} CacheLineT;

// Struct for set of cache lines
typedef struct {
    CacheLineT* lines; // Lines inside each set
    int NumLines; // Number of lines
} CacheSetT;


typedef struct {
    CacheSetT* sets; // Sets inside the cache
    int hits; // Counts hits
    int misses; // Counts misses
    int evictions; // Counts evictions
    int blockSize; // block size for each cacheline; B = 2^b
    int NumSets; // number of sets; S = 2^s
    int NumLines; // number of lines in each set
    int CacheSize; // Cache = S * B * E
} CacheT;

// global variables set by command line argument
int hflag = 0;
int vflag = 0;
int s = 0;
int E = 0;
int b = 0;
char* trace;
int timestamp = 1;

int S; // number of sets; S = 2^s
int B; // block size in bytes; B = 2^b

// Cache we are simulating on
CacheT cache1;

// Initializes the cache based on command line arguments
void initCache(){
    // Create a new set and line to put into cache1
    CacheSetT CacheSet;
    CacheLineT CacheLine;
    
    // Cache block size, sets, lines, and size of cache
    cache1.blockSize = B;
    cache1.NumSets = S;
    cache1.NumLines = E;
    cache1.CacheSize = B * S * E;
    // Allocates memory for the sets of cache1
    cache1.sets = (CacheSetT*) malloc(S * sizeof(CacheSetT));
    
    // If cach1.sets is NULL, then we cannot allocate that many sets
    if (cache1.sets == NULL){
        fprintf(stderr, "Cannot allocate sets");
        exit(1);
    }
    
    // For loop that loads the number of lines and allocates space in our CacheSet
    // Then puts that set into cache1 and increments i
    for (int i = 0; i < cache1.NumSets; i++){
        CacheSet.NumLines = E;
        CacheSet.lines = (CacheLineT*) malloc(E * sizeof(CacheLineT));
        cache1.sets[i] = CacheSet;
        
        // If the CacheSet lines == NULL, we cannot allocate that many lines
        if (CacheSet.lines == NULL){
            fprintf(stderr, "Cannot allocate line for set %d", i);
            exit(1);
        }
        
        // For loop that sets the valid, tag, and timestamp of CachLine and then loads
        // those lines into the cache set and increments j
        for (int j = 0; j < E; j++){
            CacheLine.valid = 0;
            CacheLine.tag = 0;
            CacheLine.timestamp = 0;
            CacheSet.lines[j] = CacheLine;
        }
    }
    
    // We now have an empty cache based on the command line inputs
    return;
}

// Frees cache from memory. Frees the lines of each set then the set itself
void freeCache(){
    for (int i = 0; i < cache1.NumSets; i++){
        free(cache1.sets[i].lines);
    }
    free(cache1.sets);
    return;
}

// Accesses the cache at
void cacheData(MemAddrT address){
    
    // Variables
    CacheSetT set;
    int index = 0;
    // Assume cache set is full, so set to 1
    int IsFull = 1;
    int bits = (s + b);
    int TagSize = (64 - bits);
    int Hits = cache1.hits;
    
    // Find which set to access
    unsigned long long SetIndex = (address << TagSize) >> (TagSize + b);
    // The tag bits of loaded address
    unsigned long long Tag = address >> (b + s);
    
    // Set the set variable to the set within our cache
    set = cache1.sets[SetIndex];
    
    // For loop repeats based on the number of lines in the cache
    for (int i = 0; i < cache1.NumLines; i++){
        // check to see if this line is valid
        if (set.lines[i].valid){
            // If tags are the same, then increment hits and add a timestamp and
            // increment the timestamp
            if (set.lines[i].tag == Tag){
                set.lines[i].timestamp = timestamp;
                cache1.hits++;
                timestamp++;
            }
        }
        // Set IsFull variable to 0
        else if (IsFull){
            IsFull = 0;
        }
    }
    
    // If we still have the same number of hits, then it was not valid and we had a
    // miss, so increment misses
    if (cache1.hits == Hits){
        cache1.misses++;
    } else{
        return;
    }
    
    // If the cache is full than evict the least recently used line
    if (IsFull){
        int LRU = 0;
        
        // least recently used is based of timestamps; just use line 0 to start
        LRU = set.lines[0].timestamp;
        
        for (int i = 0; i < set.NumLines; i++){
            // If the current LRU is greater than the one at line[i], then the
            // the one at line[i] is the LRU
            if (LRU > set.lines[i].timestamp){
                LRU = set.lines[i].timestamp;
                // the index of the line that is LRU is i
                index = i;
            }
        }
        // increment evictions
        cache1.evictions++;
        // set the tag of the line at index to the new tag
        set.lines[index].tag = Tag;
        // Give this line a new timestamp then increment timestamp
        set.lines[index].timestamp = timestamp;
        timestamp++;
    }
    // If the cache is not full then find the next empty line in the set
    else{
        int i;
        // finds the next non valid line and gets its index within the set
        for (i = 0; i < set.NumLines; i++){
            if (set.lines[i].valid){
                continue;
            }
            else{
                index = i;
                break;
            }
        }
        index = i;
        
        // sets the valid, tag, and timestamp of new line
        set.lines[index].valid = 1;
        set.lines[index].tag = Tag;
        set.lines[index].timestamp = timestamp;
        timestamp++;
    }
    
    return;
}

// Reads in the trace file to be simulated
void Trace(char *TraceFile){
    // Variables
    FILE *File; // The file we will be simulating the cache memory on
    char AccessOp; // Type of access operation
    MemAddrT address;
    int size;
    
    // Open the trace file and exit if it failed to be open
    if ((File = fopen(TraceFile, "r")) == NULL){
        fprintf(stderr, "Cannot open file");
        exit(1);
    }
    
    // Read each line of the file one at a time until there are no more lines
    while (fscanf(File, " %c %llx,%d", &AccessOp, &address, &size)>0){
        // Cases for different access commands
        switch(AccessOp){
            case 'M':
                cacheData(address);
                cacheData(address);
                break;
            case 'L':
                cacheData(address);
                break;
            case 'S':
                cacheData(address);
                break;
        }
    }
    
}



int main(int argc, char** argv){
    int opt;
    opterr = 0;
    
    // Reads in arguments
    while(-1 != (opt = getopt(argc, argv, "hvs:E:b:t:"))){
        switch(opt){
            case 'h':
                hflag = 1;
                break;
            case 'v':
                vflag = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                trace = optarg;
                break;
            default:
                printf("wrong argument\n");
                break;
        }
    }
    if (s == 0 || E == 0 || b == 0 || trace == NULL){
        printf("Incorrect command line arguments\n");
        exit(1);
    }
    
    // Calculates S and B
    S = pow(2, s);
    B = pow(2, b);
    
    // create the cache
    initCache();
    
    // simulate cache on the input valgrind trace
    Trace(trace);
    
    // print the summary of the cache
    printSummary(cache1.hits, cache1.misses, cache1.evictions);
    
    // free the cache memory
    freeCache();
    
    return 0;
}
