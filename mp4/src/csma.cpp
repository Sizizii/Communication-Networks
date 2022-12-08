#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>

using namespace std;

int numNodes = 0;
int pktSize = 0;
int retransMax = 0;
int totalTime = 0;

vector<int> ranges;

typedef struct node
{
    int R;      // initialize as 4, double when collision, reset when count == M
    int backoff;    // backoff time
    int count;  // count its transmissions' collision
}node;


void readParameters(char* filename){
    std::ifstream ifs;
    ifs.open(filename, ios::in);
    
    /* read fail*/
    if (!ifs.is_open())
    {
      cout << "read fail." << endl;
      exit(1);
    }
    string buf;
    stringstream s;
    string N, L, M, R, T;
    string value;
    string lineType;
    while (getline(ifs, buf)){
        s.clear();
        s.str(buf);
        s >> lineType >> value;
        if (lineType == "N"){
            numNodes = stoi(value);
        }else if(lineType == "L"){
            pktSize = stoi(value);
        }else if(lineType == "M"){
            retransMax = stoi(value);
        }else if(lineType == "T"){
            totalTime = stoi(value);
        }else if(lineType == "R"){
            ranges.push_back(stoi(value));
            string item;
            while(s >> item){
                ranges.push_back(stoi(item));
            }
        }
    }
}


int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    readParameters(argv[1]);

    /* initialize time */
    int timetick = 0;
    int tran_slots = 0;

    /* initialize nodes */
    vector<node> nodesvec(numNodes);
    for (int nodeidx = 0; nodeidx < numNodes; nodeidx++)
    {
        nodesvec[nodeidx].R = ranges[0];
        nodesvec[nodeidx].backoff = nodeidx % nodesvec[nodeidx].R;
        nodesvec[nodeidx].count = 0;
    }

    /* start simulation */
    while (timetick < totalTime)
    {
        /* first loop through all nodes to check if a node is ready to transmit*/
        vector<int> channel0nodes;
        for (int nodeidx = 0; nodeidx < numNodes; nodeidx++){
            if (nodesvec[nodeidx].backoff == 0){ channel0nodes.push_back(nodeidx); }
        }

        /* three cases here: 0 nodes ready, 1 nodes for success transmission, and >= 2 for collision*/
        int channel_size = channel0nodes.size();
        switch (channel_size)
        {
            case 0:
                {
                    /* no nodes ready for transmission, all backoff values decrease by 1 and continue simulation*/
                    timetick++;
                    for (int nodeidx = 0; nodeidx < numNodes; nodeidx++){
                        nodesvec[nodeidx].backoff--;
                    }
                }
                    break;

            case 1:
                {
                    /* when only 1 node ready for transmission, succeed! all other nodes froze, increase timetick, tran_slots */
                    timetick += pktSize;
                    tran_slots += pktSize;
                    /* reset node's parameters */
                    int nodeidx = channel0nodes[0];
                    nodesvec[nodeidx].R = ranges[0];
                    nodesvec[nodeidx].count = 0;
                    nodesvec[nodeidx].backoff = (nodeidx + timetick) % nodesvec[nodeidx].R;
                }
                    break;

            default:
                {
                    timetick++;
                    for (int iteridx = 0; iteridx < channel_size; iteridx++){
                        int nodeidx = channel0nodes[iteridx];
                        nodesvec[nodeidx].count++;
                        if (nodesvec[nodeidx].count == retransMax){ nodesvec[nodeidx].count = 0; }
                        nodesvec[nodeidx].R = ranges[nodesvec[nodeidx].count];
                        nodesvec[nodeidx].backoff = (nodeidx + timetick) % nodesvec[nodeidx].R;
                    }
                }
                    break;
        }
    }

    float rate = (tran_slots - (timetick - totalTime))/(float)totalTime;
    rate = round(rate*100)/100.0;
    ofstream outfile;
    outfile.open("output.txt");
    outfile.setf(ios::fixed);
    outfile << setprecision(2) << rate << endl;
    outfile.close();
    // FILE *fpOut;
    // fpOut = fopen("output.txt", "w");
    // fwrite(&rate, sizeof(float), 1, fpOut);
    // fclose(fpOut);

    return 0;
}

