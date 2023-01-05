#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<map>
#include<set>
#include<fstream>
#include<limits.h>
#include<list>
#include <iostream>

using namespace std;

string topofile, messagefile, changesfile;
map<int, map<int, int> > topology; // <source node, < dest node, nexthop, cost>>
map<int, map<int, pair<int, int> > > forwarding_table; // <node, <destination, <nexthop, pathcost> > >
set<int> nodes;
typedef struct message{
    int source;
    int destination;
    string message;
} message;
list<message> message_list;
ofstream fpOut;

void initial(string topo_file, string message_file){
    ifstream read_topology;
    read_topology.open(topo_file, ios::in);

    //record the original topology
    int source_node, dest_node, cost;
    while(read_topology >> source_node >> dest_node >> cost){       
        topology[source_node][dest_node] = cost;
        topology[dest_node][source_node] = cost;

        if (nodes.count(source_node) == 0) nodes.insert(source_node);
        if (nodes.count(dest_node) == 0) nodes.insert(dest_node);
    }
    
    read_topology.close();

    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            int destination = itt;
            if (destination == source){
                forwarding_table[source][destination] = make_pair(destination, 0);
                topology[source][destination] = 0;
            } else if (!topology[source].count(destination)){
                forwarding_table[source][destination] = make_pair(destination, 10000);
                topology[source][destination] = 10000;
            } else{
                forwarding_table[source][destination] = make_pair(source, topology[source][destination]);
            }
        }
    }
    /*
    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            int destination = itt;
            cout << forwarding_table[source][destination].first<<" "<<forwarding_table[source][destination].second << endl;
        }
        cout << endl;
    }
    */
    //cout << "here4" <<endl;
    map<int, bool> visited_node;
    map<int,int> path;
    //map<int,int> path_cost;
    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            visited_node[itt] = false;
            path[itt] = -1;
            //path_cost[itt] = 10000;
        }
        int node_now = source;
        int min_cost = 0;
        visited_node[node_now] = true;
        for (int i = 0; i < nodes.size(); ++i){
            for (auto itt:nodes){
                int dest = itt;
                if (visited_node[dest] == true) continue;
                if (topology[node_now][dest] == 10000) continue;
                if (min_cost + topology[node_now][dest] < forwarding_table[source][dest].second){
                    forwarding_table[source][dest] = make_pair(node_now, min_cost + topology[node_now][dest]); 
                } else if (min_cost + topology[node_now][dest] == forwarding_table[source][dest].second &&
                           node_now < forwarding_table[source][dest].first){
                    forwarding_table[source][dest] = make_pair(node_now, min_cost + topology[node_now][dest]);
                } 
            }
            int node_next;
            min_cost = 10000;
            for (auto itt:nodes){
                int dest = itt;
                if (visited_node[dest] == true) continue;
                if (forwarding_table[source][dest].second < min_cost){
                    min_cost = forwarding_table[source][dest].second;
                    node_next = dest;
                }
            }
            node_now = node_next;
            visited_node[node_now] = true;
        }
        //cout << forwarding_table[3][2].first << endl;
        //cout << "here6" <<endl;
        for (auto itt:nodes){
            int dest = itt;
            int fir = itt;
            while(forwarding_table[source][fir].first != source){
                fir = forwarding_table[source][fir].first;
            }
            path[dest] = fir;
        }
        //cout << "here7" <<endl;
        for(auto itt:nodes){
            int destination = itt;
            forwarding_table[source][destination].first = path[destination];
            if (forwarding_table[source][destination].second == 10000) continue;
            fpOut << destination << " " << forwarding_table[source][destination].first << " " << forwarding_table[source][destination].second << endl;
        }
            
    }
    
    //cout << "here5" <<endl;
    //get message
    ifstream read_message;
    read_message.open(message_file);
    string one_messsage, source, dest;
    int s, d;
    while (getline(read_message, one_messsage)){
        int index = one_messsage.find(" ");
        source = one_messsage.substr(0,index);
        one_messsage = one_messsage.substr(index+1);
        index = one_messsage.find(" ");
        dest = one_messsage.substr(0,index);
        one_messsage = one_messsage.substr(index+1);
        s = atoi(source.c_str());
        d = atoi(dest.c_str());
        message newmessage;
        newmessage.source = s;
        newmessage.destination = d;
        newmessage.message = one_messsage;
        message_list.push_back(newmessage);
    }
    read_message.close();

}

void send_message(){
    int source, dest, cost, next_hop;
    list<message>::iterator it = message_list.begin();
    for (; it != message_list.end(); it++){
        source = it->source;
        dest = it->destination;
        next_hop = it->source;
        cost = forwarding_table[source][dest].second;
        if (cost == 10000){
            fpOut <<"from "<<source<<" to "<<dest<<" cost "<<"infinite hops unreachable ";
        }else {
            fpOut <<"from "<<source<<" to "<<dest<<" cost "<<cost<<" hops ";
            while (next_hop != dest) {
                fpOut << next_hop << " ";
                next_hop = forwarding_table[next_hop][dest].first;
            }
        }
        fpOut << "message " << it->message<< endl;
    }
    
}

void dochanges(){
    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            int destination = itt;
            if (destination == source){
                forwarding_table[source][destination] = make_pair(destination, 0);
            } else if (!topology[source].count(destination)){
                forwarding_table[source][destination] = make_pair(destination, 10000);
            } else{
                forwarding_table[source][destination] = make_pair(source, topology[source][destination]);
            }
        }
    }

    map<int, bool> visited_node;
    map<int,int> path;
    //map<int,int> path_cost;
    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            visited_node[itt] = false;
            path[itt] = -1;
            //path_cost[itt] = 10000;
        }
        int node_now = source;
        int min_cost = 0;
        visited_node[node_now] = true;
        for (int i = 0; i < nodes.size(); ++i){
            for (auto itt:nodes){
                int dest = itt;
                if (visited_node[dest] == true) continue;
                if (topology[node_now][dest] == 10000) continue;
                if (min_cost + topology[node_now][dest] < forwarding_table[source][dest].second){
                    forwarding_table[source][dest] = make_pair(node_now, min_cost + topology[node_now][dest]); 
                } else if (min_cost + topology[node_now][dest] == forwarding_table[source][dest].second &&
                           node_now < forwarding_table[source][dest].first){
                    forwarding_table[source][dest] = make_pair(node_now, min_cost + topology[node_now][dest]);
                }
            }
            int node_next;
            min_cost = 10000;
            for (auto itt:nodes){
                int dest = itt;
                if (visited_node[dest] == true) continue;
                if (forwarding_table[source][dest].second < min_cost){
                    min_cost = forwarding_table[source][dest].second;
                    node_next = dest;
                }
            }
            node_now = node_next;
            visited_node[node_now] = true;
        }
        for (auto itt:nodes){
            int dest = itt;
            int fir = itt;
            while(forwarding_table[source][fir].first != source){
                fir = forwarding_table[source][fir].first;
            }
            path[dest] = fir;
        }

        for(auto itt:nodes){
            int destination = itt;
            forwarding_table[source][destination].first = path[destination];
            if (forwarding_table[source][destination].second == 10000) continue;
            fpOut << destination << " " << forwarding_table[source][destination].first << " " << forwarding_table[source][destination].second << endl;
        }
  
    }

    send_message();

}


int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    
    topofile = argv[1];
    messagefile = argv[2];
    changesfile = argv[3];

    fpOut.open("output.txt");
    //cout << "here1" <<endl;
    initial(topofile, messagefile);
    //cout << "here2" <<endl;
    send_message();
    //cout << "here3" <<endl;

    ifstream read_changes;
    read_changes.open(changesfile, ios::in);
    int source_node, dest_node, cost;
    while(read_changes >> source_node >> dest_node >> cost){
        if (cost == -999) cost = 10000;
        topology[source_node][dest_node] = cost;
        topology[dest_node][source_node] = cost;
        dochanges();
    }
    read_changes.close();

    fpOut.close();
    //cout << "here5" <<endl;

    //FILE *fpOut;
    //fpOut = fopen("output.txt", "w");
    //fclose(fpOut);
    

    return 0;
}

