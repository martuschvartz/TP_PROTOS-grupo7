#include "metrics.h"

static unsigned int hConnections = 0;
static unsigned int cConnections = 0;
static unsigned int bytes = 0;

void moreConnections(){ 
    hConnections++;
    cConnections++;
}

void lessConnections(){  
    cConnections--;
}

unsigned int getHistoricalConnections(){
    return hConnections;
}

unsigned int getConnections(){
    return cConnections;
}

void moreBytes(unsigned int b){
    bytes+=b;
}

unsigned int getBytes(){
    return bytes;
}
