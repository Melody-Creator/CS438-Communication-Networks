#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>

int N, L, R, M, T;

class node {
public:
    node(int id, int R) {
        this->backoff = 0;
        this->collision = 0;
        this->ID = id;
        this->R = R;
    }
    void setBackoff(int t) {
        this->backoff = (this->ID + t) % this->R;
    }
    int backoff;
    int collision;
    int ID;
    int R;
};

std::vector<int> R_list;
std::vector<node> node_list;

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    freopen(argv[1], "r", stdin);
    freopen("output.txt", "w", stdout);
    
    // read from the input file
    char para;
    std::cin >> para >> N;
    std::cin >> para >> L;
    std::cin >> para >> M;
    std::cin >> para;
    for (int i = 0; i < M; i++) {
        std::cin >> R;
        R_list.push_back(R);
    }
    
    std::cin >> para >> T;
    
    // initialize
    for (int i = 0; i < N; i++) {
        node_list.emplace_back(node(i, R_list[0]));
        node_list[i].setBackoff(0);
    }
    
    int transmitted_time = 0;  // the number of slots where a packet is transmitted without collision

    for (int t = 0; t < T; t++) {

        // find the node that should be send
        std::vector<int> send_list;
        for (int i = 0; i < N; i++) {
            if (node_list[i].backoff == 0)
                send_list.push_back(i);
        }
        
        if (send_list.size() == 0) {                 // no packet to send
            for (int i = 0; i < N; i++)  node_list[i].backoff --;
        } else if (send_list.size() > 1) {           // collision
            for (auto send : send_list) {
                int& collision = node_list[send].collision;
                collision ++;
                if (collision == M)  collision = 0;

                node_list[send].R = R_list[collision];
                node_list[send].setBackoff(t + 1);
            }
        } else {                                     // one packet to send
            if (t + L <= T) {
                t += L - 1;
                transmitted_time += L;
                node_list[send_list[0]].collision = 0;
                node_list[send_list[0]].R = R_list[0];
                node_list[send_list[0]].setBackoff(t + 1);
            } else {
                transmitted_time += T - t;
                break;
            }
        }
    }


    printf("%.2f\n", 1.0 * transmitted_time / T);

    fclose(stdin);
    fclose(stdout);

    return 0;
}
