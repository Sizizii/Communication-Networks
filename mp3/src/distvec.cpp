#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <limits.h>
#include <vector>
#include <algorithm>
using namespace std;

#define INFINITY_COST INT_MAX/2

// source, destination, cost
unordered_map<int, unordered_map<int, int> > topoMap;
// node, destination, nexthop, cost
unordered_map<int, unordered_map<int, pair<int, int> > > fTable;  

set<int> nodes;

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
        if (nodes.find(srcInt) == nodes.end()){
          nodes.insert(srcInt);
        }
        if (nodes.find(destInt) == nodes.end()){
          nodes.insert(destInt);
        }
        /* update costs */
        topoMap[srcInt][destInt] = costInt;
        topoMap[destInt][srcInt] = costInt;
    }
    ifs.close();
}

void distvec(){
    // implement algorithm to produce fTable from topoMap
    /* the first is child, the second is a pair: first is predecessor, and second is cost */
    /* We don't include mapping for source node in nodePairs */
    unordered_map<int, map<int, int> > costPairs; /* the first int is src, second int is dst, third is cost */
    unordered_map<int, map<int, int> > hopPairs; /* the first int is src, second int is dst, third is next hop */
    fTable.clear();

    /*initialization*/
    for (set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++)
    {
      int srcInt = *srciter;
      for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
        int dstInt = *dstiter;

        /* same src and dst*/
        if (*dstiter == *srciter)
        {
          // nodePairs[dstInt].first = srcInt;
          // nodePairs[dstInt].second = 0;
          fTable[srcInt][dstInt].first = dstInt;
          fTable[srcInt][dstInt].second = 0;
          costPairs[srcInt][dstInt] = 0;
          hopPairs[srcInt][dstInt] = dstInt;
        }
        
        /* if neighbors */
        else if (topoMap[srcInt].find(dstInt) != topoMap[srcInt].end())
        {
          costPairs[srcInt][dstInt] = topoMap[srcInt][dstInt];
          hopPairs[srcInt][dstInt] = dstInt;
        }
        /* nonadjacent */
        else{
          costPairs[srcInt][dstInt] = INFINITY_COST;
          hopPairs[srcInt][dstInt] = INFINITY_COST;
        }
      }
    }

    cout << "Finish initialization" << "\n";

    unordered_map<int, map<int, int> > costPairs_modify = costPairs; 
    unordered_map<int, map<int, int> > hopPairs_modify = hopPairs;

      /* change tables until converge */
    int ifend = 0;
    while (ifend == 0)
    {
      int ifchange = 0;
      for (set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++){
        int srcInt = *srciter;
        for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
          int dstInt = *dstiter;
          if (srcInt == dstInt)
          {
            /* nothing have to be changed */
            continue;
          }
          for (const auto& keyvalpair: topoMap[srcInt]){
            int neighbor = keyvalpair.first;
            if (costPairs[neighbor][dstInt] != INFINITY_COST)
            {
              int cost_new = topoMap[srcInt][neighbor] + costPairs[neighbor][dstInt];
              if ((cost_new < costPairs[srcInt][dstInt]) || ((cost_new == costPairs[srcInt][dstInt]) && (neighbor < hopPairs[srcInt][dstInt])))
              {
                ifchange = 1;
                costPairs_modify[srcInt][dstInt] = cost_new;
                hopPairs_modify[srcInt][dstInt] = neighbor;
              }
            }
          }
        }
      }
      for (set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++){
        int srcInt = *srciter;
        for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
          int dstInt = *dstiter;
          costPairs[srcInt][dstInt] = costPairs_modify[srcInt][dstInt];
          hopPairs[srcInt][dstInt] = hopPairs_modify[srcInt][dstInt];
        }
      }

      if (ifchange == 1){ ifend = 0;  }
      else{ ifend = 1; }
    }
      

    cout << "Finish computing cost" << "\n";

    /* Fill up forwarding table */
    for (set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++){
      int srcInt = *srciter;
      for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
        int dstInt = *dstiter;
        dstInt = *dstiter;
        /* only insert reachable pairs in forwarding table */
        if (costPairs[srcInt][dstInt] != INFINITY_COST)
        {
          fTable[srcInt][dstInt].second = costPairs[srcInt][dstInt];
          fTable[srcInt][dstInt].first = hopPairs[srcInt][dstInt];
        }
      }
    }

    cout << "Finish filling up forwarding table \n";
    cout << "\n Finish link state algorithm \n";
}

void writeOutput(std::ofstream& out, char* messagefile){
    // write output into messageFile based on requirements
    // include 1. topology entries. 2. messages

    cout << "Starting to write output file \n";
    
    if (out.is_open())
    {
      /* output forwaring table */
      for(set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++){
        for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
          if (*dstiter == *srciter)
          {
            out << *dstiter << " " << *dstiter << " " << "0" << "\n";
          }
          else{
            /* only prints reachable destinations */
            if (fTable[*srciter].find(*dstiter) != fTable[*srciter].end())
            {
              out << *dstiter << " " << fTable[*srciter][*dstiter].first << " " << fTable[*srciter][*dstiter].second << "\n";
            }
          }
        }}

      cout << "Finish writing forwarding table \n";

      /* output messages 
       * format: 
       “from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>” 
       “from <x> to <y> cost infinite hops unreachable message <message>”
       */
      std::ifstream msgin;
      msgin.open(messagefile, ios::in);
      if (msgin.is_open())
      {
        string buf;
        //stringstream s;
        string src, dest;
        int srcInt, destInt;
        while (getline(msgin, buf)){
          int unreachable = 0;
          char msg[buf.length()-4];
          // s.clear();
          // s.str(buf);
          sscanf(buf.c_str(), "%d %d %[^\n]", &srcInt, &destInt, msg);
          // srcInt = stoi(src);
          // destInt = stoi(dest);

          /* non-exist src or dest node */
          if ((nodes.find(srcInt) == nodes.end()) || nodes.find(destInt) == nodes.end())
          {
            out << "from " << srcInt << " to " << destInt << " cost infinite hops unreachable message " << msg <<"\n";
            continue;
          }
          
          /* traverse its path */
          vector<int> msgHops;
          int cur_node = srcInt;
          while (cur_node != destInt)
          {
            msgHops.push_back(cur_node);
            if (fTable[cur_node].find(destInt) == fTable[cur_node].end())
            {
              unreachable = 1;
              break;
            }
            cur_node = fTable[cur_node][destInt].first;
          }

          /* if path complete*/
          if (unreachable == 1)
          {
            out << "from " << srcInt << " to " << destInt << " cost infinite hops unreachable message " << msg <<"\n";
            continue;
          }

          out << "from " << srcInt << " to " << destInt << " cost " << fTable[srcInt][destInt].second << " hops ";
          /* output hops */
          for (std::vector<int>::iterator hopiter = msgHops.begin(); hopiter != msgHops.end(); hopiter++){
	          out << *hopiter << " ";
          }
          out << "message " << msg << "\n";
        }
        msgin.close();
      }
      else{
        cout << "read message fail." << endl;
        exit(1);
      }

      cout << "Finish writing messages \n";
    }
    else{
      cout << "write output fail." << endl;
      exit(1);
    }
    
}

void changesFile(std::ofstream& out, char* changesfile, char* messagefile){
    // read changesfile to change topoMap
    ifstream ifs;
    ifs.open(changesfile, ios::in);
    if (!ifs.is_open())
    {
        cout << "read fail." << endl;
        exit(1);
    }

    cout << "\n Start chaging topo \n";

    string buf;
    while (getline(ifs, buf))
    {
      cout << "\n Change \n";
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
      distvec();
      writeOutput(out, messagefile);
    }
    ifs.close();
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    std::ofstream out;
    out.open("output.txt");
    
    readTopoFile(argv[1]);
    distvec();
    writeOutput(out, argv[2]);
    // big loop
    // stop when changesfile exhausted
    changesFile(out, argv[3], argv[2]); 

    out.close();
    return 0;
}

