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
        //if (! read_topology >> source_node) break;
        //read_topology >> dest_node;
        //read_topology >> cost;
        
        topology[source_node][dest_node] = cost;
        topology[dest_node][source_node] = cost;

        if (nodes.count(source_node) == 0) nodes.insert(source_node);
        if (nodes.count(dest_node) == 0) nodes.insert(dest_node);
    }
    //cout << "here4" <<endl;
    
    read_topology.close();

    for(auto it:nodes){
        int source = it;
        for (auto itt:nodes){
            int destination = itt;
            if (destination == source){
                forwarding_table[source][destination] = make_pair(destination, 0);
            } else if (!topology[source].count(destination)){
                forwarding_table[source][destination] = make_pair(destination, 10000);
            } else{
                forwarding_table[source][destination] = make_pair(destination, topology[source][destination]);
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
    int node_num = nodes.size();
    for (int i = 0; i < node_num; ++i){
        for(auto it:nodes){
            int source = it;
            for(auto itt:nodes){
                int destination = itt;
                for(auto ittt:nodes){
                    int min_cost = forwarding_table[source][destination].second;
                    int node_now = ittt;
                    int cost_now = forwarding_table[source][node_now].second + forwarding_table[node_now][destination].second;
                    if(cost_now < min_cost){
                        //cout<<source<<" "<<node_now<<" "<<destination<<endl;
                        //cout<<cost_now<<" "<<min_cost<<endl;
                        //cout<<endl;
                        forwarding_table[source][destination] = make_pair(node_now, cost_now);
                    } else if (cost_now == min_cost && node_now != source){
                        int next_hop = forwarding_table[source][destination].first;
                        if (forwarding_table[source][next_hop].second > forwarding_table[source][node_now].second){
                            forwarding_table[source][destination] = make_pair(node_now, cost_now);
                        }
                    }
                }
            }
        }
    }

    //write forwarding table
    for(auto it:nodes){
        int source = it;
        for(auto itt:nodes){
            int destination = itt;
            fpOut << destination << " " << forwarding_table[source][destination].first << " " << forwarding_table[source][destination].second << endl;
        }
        //fpOut << endl;
    }
    
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
                forwarding_table[source][destination] = make_pair(destination, topology[source][destination]);
            }
        }
    }
    
    //get forwarding table
    int node_num = nodes.size();
    for (int i = 0; i < node_num; ++i){
        for(auto it:nodes){
            int source = it;
            for(auto itt:nodes){
                int destination = itt;
                for(auto ittt:nodes){
                    int min_cost = forwarding_table[source][destination].second;
                    int node_now = ittt;
                    int cost_now = forwarding_table[source][node_now].second + forwarding_table[node_now][destination].second;
                    if(cost_now < min_cost){
                        //cout<<source<<" "<<node_now<<" "<<destination<<endl;
                        //cout<<cost_now<<" "<<min_cost<<endl;
                        //cout<<endl;
                        forwarding_table[source][destination] = make_pair(node_now, cost_now);
                    } else if (cost_now == min_cost && node_now != source){
                        int next_hop = forwarding_table[source][destination].first;
                        if (forwarding_table[source][next_hop].second > forwarding_table[source][node_now].second){
                            forwarding_table[source][destination] = make_pair(node_now, cost_now);
                        }
                    }
                }
            }
        }
    }

    //write forwarding table
    for(auto it:nodes){
        int source = it;
        for(auto itt:nodes){
            int destination = itt;
            fpOut << destination << " " << forwarding_table[source][destination].first << " " << forwarding_table[source][destination].second << endl;
        }
    }

    send_message();

}


int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
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
        //if (! read_changes >> source_node) break;
        //read_changes >> dest_node;
        //read_changes >> cost;
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

