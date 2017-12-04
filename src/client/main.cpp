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

using namespace enetpp;
using namespace std;
static const int PORT = 8082;

int main(int argc, char** argv) {
    

	int fd = open("/home/liuwei/code/xiaomi/stream/pandora/P2P/p2pengine/bin/htdocs/1.mp4_0",
		 O_CREAT | O_RDWR | O_APPEND, 0666);
    if(fd==-1) printf("open error:%d\n",errno),exit(-1);

    char buff[1 * 1024 * 1024];
    int a = 11;
    int r = read(fd, buff, 512* 1204);//从虚拟内存的相同地址中，读取实际地址位置相同的数据到data中

    cout<< " read data len="<< r << "\n";

    EnetSocket eSocket;
    eSocket.bind(PORT);

    eSocket.write(buff, r, "127.0.0.1", 8081);
    
    // eSocket.connect("127.0.0.1", 8081);
    
    sleep(5000);
    
    
    eSocket.close();
    return 0;
}