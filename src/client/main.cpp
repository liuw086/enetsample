//
//  main.cpp
//  enetServer
//
//  Created by LiuWei on 2017/12/1.
//  Copyright © 2017年 LiuWei. All rights reserved.
//

#include <iostream>
#include <string>
#include <random>
#include <stdio.h>
#include "enetpp/server.h"
#include "enetpp/global_state.h"
#include "enet_socket.hpp"
#include <fcntl.h>
#include <vector>

using namespace enetpp;
using namespace std;
static const int PORT = 8082;

int main(int argc, char** argv) {
    

	int fd = open("/home/liuwei/code/xiaomi/stream/pandora/P2P/p2pengine/bin/htdocs/1.mp4_0",
		 O_CREAT | O_RDWR | O_APPEND, 0666);
    if(fd==-1) printf("open error:%d\n",errno),exit(-1);

    EnetSocket eSocket;


    auto on_data_received = [&](string ip, unsigned short port, const char* data, size_t data_size) {
        std::cout << "server: ==on_data_received from client" << ip<< ":"<< port << "; len="<< data_size << std::endl;
    };

    eSocket.bind(PORT, on_data_received);

    // eSocket.write(buff, r, "127.0.0.1", 8081);



    sleep(10);

    int timeout = 3000;

    vector<std::pair<string, unsigned short>> addr_list;
    addr_list.push_back(make_pair("127.0.0.1", 8081));
    addr_list.push_back(make_pair("127.0.0.1", 8083));

    for(int i = 0; i< 10; i++){

        cout<< " start_ice =====================\n";
        eSocket.start_ice(addr_list, timeout, [&](string ip, unsigned short port) {
            cout<< " on_ice_succ "<< ip << ":" << port << "\n";

            //TODO when signal ok use it
            eSocket.stopIce();

            char buff[1 * 1024 * 1024];
            int r = read(fd, buff, 512* 1204);//从虚拟内存的相同地址中，读取实际地址位置相同的数据到data中

            eSocket.write(buff, r, ip, port);
            // eSocket.connect("127.0.0.1", 8081);
        });
        while(eSocket.isIceing()){
            sleep(1);
        }
    }
    sleep(1);
    eSocket.clearPeers();
	

    
    // eSocket.connect("127.0.0.1", 8081);

    sleep(5000);
    
    
    eSocket.close();
    return 0;
}