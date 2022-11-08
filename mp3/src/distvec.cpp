#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <unordered_set>
#include<unordered_map>
#include<iostream>
#include<fstream>
#include <string>
#include <sstream>
using namespace std;

// source, destination, cost
unordered_map<int, unordered_map<int, int> > topoMap;
// node, destination, nexthop, cost
unordered_map<int, unordered_map<int, pair<int, int> > > fTable;

unordered_set<int> nodes;

void readTopoFile(char* topofile){
    // read topofile into topoMap
    std::ifstream ifs;
    ifs.open(topofile, ios::in);

    /* read fail*/
    if (!ifs.is_open())
    {
        cout << "read fail." << endl;
        exit(1);
    }

    string buf;
    stringstream s;
    string src, dest, cost;
    int srcInt, destInt, costInt;
    while (getline(ifs, buf))
    {
        s.clear();
        s.str(buf);
        s >> src >> dest >> cost;

        /* store nodes info */
        srcInt = stoi(src);
        destInt = stoi(dest);
        costInt = stoi(cost);
        
        /* insert nodes */
        if (nodes.find(srcInt) == end(nodes)){
          nodes.insert(srcInt);
        }
        if (nodes.find(destInt) == end(nodes)){
          nodes.insert(destInt);
        }
        /* update costs */
        topoMap[srcInt][destInt] = costInt;
        topoMap[destInt][srcInt] = costInt;
    }
    ifs.close();
}

void distVec(){
    // implement algorithm to produce fTable from topoMap
}

void writeOutput(char* messagefile){
    // write output into messageFile based on requirements
    // include 1. topology entries. 2. messages
}

void changesFile(char* changesfile, char* messagefile){
    // read changesfile to change topoMap
    ifstream ifs;
    ifs.open(changesfile, ios::in);
    if (!ifs.is_open())
    {
        cout << "read fail." << endl;
        exit(1);
    }
    string buf;
    while (getline(ifs, buf))
    {
        string src, dest, cost;
        int srcInt, destInt, costInt;
        stringstream s(buf);
        s >> src >> dest >> cost;
        srcInt = stoi(src);
        destInt = stoi(dest);
        costInt = stoi(cost);
        if (costInt == -999){
            topoMap[srcInt].erase(destInt);
            topoMap[destInt].erase(srcInt);
            if(topoMap.find(srcInt) == topoMap.end()){
                nodes.erase(srcInt);
            }
            if(topoMap.find(destInt) == topoMap.end()){
                nodes.erase(destInt);
            }
        }else{
            topoMap[srcInt][destInt] = costInt;
            topoMap[destInt][srcInt] = costInt;
            if (nodes.find(srcInt) == end(nodes)){
                nodes.insert(srcInt);
            }
            if (nodes.find(destInt) == end(nodes)){
                nodes.insert(destInt);
            }
        }
        distVec();
        writeOutput(messagefile);
    }
    ifs.close();
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    readTopoFile("topofile");
    distVec();
    writeOutput("messagefile");
    // big loop
    // stop when changesfile exhausted
    changesFile("changesfile", "messagefile"); 
    fclose(fpOut);
    
    return 0;
}

