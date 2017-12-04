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

using namespace enetpp;
static const int PORT = 8082;

int main(int argc, char** argv) {
    

    EnetSocket eSocket;
    eSocket.bind(PORT);
    
    eSocket.connect("127.0.0.1", 8088);
    
    sleep(5);
    
    
    eSocket.close();
    return 0;
}
