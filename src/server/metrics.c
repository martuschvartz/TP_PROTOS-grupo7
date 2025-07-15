#include "metrics.h"
#include <stdio.h>

static unsigned int hConnections = 0;
static unsigned int cConnections = 0;
unsigned int bytes = 0;

void more_connections(){ 
    hConnections++;
    cConnections++;
}

void less_connections(){  
    cConnections--;
}

unsigned int get_historical_connections(){
    return hConnections;
}

unsigned int get_connections(){
    return cConnections;
}

void more_bytes(unsigned int b){
    fprintf(stdout, "Agegamos bytes %d \n", b);
    bytes+=b;
}

unsigned int get_bytes(){
    return bytes;
}
