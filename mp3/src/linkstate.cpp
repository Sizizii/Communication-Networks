#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <limits.h>
#include <vector>
#include <algorithm>
using namespace std;

#define INFINITY_COST INT_MAX

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

void linkState(){
    // implement algorithm to produce fTable from topoMap
    unordered_set<int> nodeSet;
    /* the first is child, the second is a pair: first is predecessor, and second is cost */
    /* We don't include mapping for source node in nodePairs */
    unordered_map<int, pair<int, int> > nodePairs;

    for (set<int>::iterator srciter=nodes.begin(); srciter!=nodes.end(); srciter++)
    {
      nodeSet.clear();
      nodePairs.clear();

      nodeSet.insert(*srciter);
      int srcInt = *srciter;
      int min_cost = INFINITY_COST;
      int min_node, dstInt;

      /* initialization */
      for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
        dstInt = *dstiter;

        /* same src and dst*/
        if (*dstiter == *srciter)
        {
          // nodePairs[dstInt].first = srcInt;
          // nodePairs[dstInt].second = 0;
          fTable[srcInt][dstInt].first = dstInt;
          fTable[srcInt][dstInt].second = 0;
        }
        
        /* if neighbors */
        else if (topoMap[srcInt].find(dstInt) != topoMap[srcInt].end())
        {
          nodePairs[dstInt].first = srcInt;
          nodePairs[dstInt].second = topoMap[srcInt][dstInt];
          if (topoMap[srcInt][dstInt] < min_cost){
            min_cost = topoMap[srcInt][dstInt];
            min_node = dstInt;
          }
          else if(topoMap[srcInt][dstInt] == min_cost){
            min_node = dstInt;
          }
        }
        /* nonadjacent */
        else{
          nodePairs[dstInt].first = srcInt;
          nodePairs[dstInt].second = INFINITY_COST;
        }
      }

      cout << "Finish initialization for node " << srcInt << "\n";

      /* find costs from src to each node*/
      int test_node;
      nodeSet.insert(min_node);
      if (nodeSet.size() != 2)
      {
        cout << "Initialize error for node " << srcInt << "\n";
      }
      else{
        cout << "Correct initialization " << srcInt << "\n";
      }

      while (nodeSet.size() != nodes.size())
      {
        test_node = min_node;
        min_cost = INFINITY_COST;

        /* update for nodes not in nodeSet */
        for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){

          /* only update costs for nodes not in nodeSet */
          if (find(nodeSet.begin(), nodeSet.end(), *dstiter) == nodeSet.end())
          {
            dstInt = *dstiter;

            /* work on adjacent nodes of test_node */
            if (topoMap[test_node].find(dstInt) != topoMap[test_node].end())
            {
              /* only update when the new cost is no larger than old cost*/
              if (nodePairs[dstInt].second > (nodePairs[test_node].second + topoMap[test_node][dstInt]))
              {
                nodePairs[dstInt].first = test_node;
                nodePairs[dstInt].second = nodePairs[test_node].second + topoMap[test_node][dstInt];
              }

              else if (nodePairs[dstInt].second == (nodePairs[test_node].second + topoMap[test_node][dstInt]))
              {
                /* prefer smaller last node before destination */
                if (nodePairs[dstInt].first > test_node)
                {
                  /* test node has smaller id */
                  nodePairs[dstInt].first = test_node;
                }
              }
            }
          }

          if (nodePairs[dstInt].second < min_cost){
            min_cost = nodePairs[dstInt].second;
            min_node = dstInt;
          }
          else if (nodePairs[dstInt].second == min_cost){
            /* choose the lowest node ID to insert */
            if (dstInt < min_node)
            {
              min_node = dstInt;
            }
          }
        }

        nodeSet.insert(min_node);
      }

      cout << "Finish computing cost for node " << srcInt << "\n";

      /* Fill up forwarding table */
      for(set<int>::iterator dstiter=nodes.begin(); dstiter!=nodes.end(); dstiter++){
        // pair<int, int> temp_pair;
        if (*dstiter != srcInt)
        {
          dstInt = *dstiter;
          /* only insert reachable pairs in forwarding table */
          if (nodePairs[dstInt].second != INFINITY_COST)
          {
            fTable[srcInt][dstInt].second = nodePairs[dstInt].second;

            /* find next hop */
            int next_hop = dstInt;
            int prev_hop = nodePairs[dstInt].first;
            while (prev_hop != srcInt)
            {
              next_hop = prev_hop;
              prev_hop = nodePairs[next_hop].first;
            }
            fTable[srcInt][dstInt].first = next_hop;
          }
        }
      }

      cout << "Finish filling up forwarding table for node " << srcInt << "\n";
      
    }
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
            if (fTable[*srciter][*dstiter].second != INFINITY_COST)
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
      linkState();
      writeOutput(out, messagefile);
    }
    ifs.close();
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    std::ofstream out;
    out.open("output.txt");
    
    readTopoFile(argv[1]);
    linkState();
    writeOutput(out, argv[2]);
    // big loop
    // stop when changesfile exhausted
    changesFile(out, argv[3], argv[2]); 

    out.close();
    return 0;
}

