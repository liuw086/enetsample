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
#include "rc_utils.h"

using namespace enetpp;
static const int PORT = 8081;

int main(int argc, char** argv) {
    

    EnetSocket eSocket;
    

    auto on_data_received = [&](string ip, unsigned short port, const char* data, size_t data_size) {
        string filename = "./" + ip;
        std::ofstream o(filename.c_str(), ofstream::app); 
        o.write((const char*)data, data_size);               
        flush(o);

        std::cout << "server: ==on_data_received from client" << ip<< ":"<< port << "; len="<< data_size << std::endl;
    };

    eSocket.bind(PORT, on_data_received);
    
    eSocket.connect("127.0.0.1", 8082);


    for(int i = 0; i< 100; i++){
    	std::cout << " dns: " << milive::RCUtils::DnsReslove("zbuvideo.ks3-cn-beijing.ksyun.com") << std::endl;
    	sleep(3);
    }
    
    sleep(50000);
    
    
    eSocket.close();
    return 0;
}