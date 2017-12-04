#include <iostream>
#include <string>
#include <random>
#include <stdio.h>
#include <fstream>
#include "enetpp/server.h"
#include "enetpp/global_state.h"
#include "enetpp/trace_handler.h"

using namespace enetpp;
using namespace std;

typedef enum _SendTaskStatus
{
   STATUS_NONE       = 0,  
   STATUS_SENDING    = 1,  
   STATUS_RECEIVED   = 2
} SendTaskStatus;

class SendTask {
public:
    std::string _ip;
    unsigned short _port;
    string _sendData;
    clock_t _start;
    int _timeout;
    int _status;

public:
    SendTask(){
    }

    bool isTimeout(){
        clock_t now = clock();
        int duration = (int)((double)(now - _start) / CLOCKS_PER_SEC * 1000);
        return duration > _timeout;
    }
};

class server_client {
public:
	unsigned int _uid;
    std::string _ip;
    unsigned short _port;
    string _sendData;

public:
	server_client()
		: _uid(0) {
	}

	unsigned int get_uid() const {
		return _uid;
	}
};

class EnetSocket {
public:
    
private:
    enetpp::server<server_client> server;
    std::unique_ptr<std::thread> server_thread;
    bool s_exit = false;
    std::mutex s_cout_mutex;
    std::queue<SendTask* > sendTasklist;
private:

    std::function<void(string ip, unsigned short port)> _on_ice_succ;
    
    void checkSendTaskOnConnected(server_client& client){
        std::string ip;
        unsigned port;
        server.get_peer_uid(client.get_uid(), ip, port);

        std::cout << "server: checkSendTaskOnConnected" << ip<< ":"<< port <<"; uid="<< client.get_uid() << std::endl;

        while (!sendTasklist.empty()) {
            auto st = sendTasklist.front();
            if( st->isTimeout()){
                std::cout << "server: sendTask timeout" << st->_ip<< ":"<< st->_port << "; len="<<st->_sendData.length() << std::endl;
                sendTasklist.pop();
                delete st;
            }else if(st->_ip == ip && st->_port == port ){
                st->_status = STATUS_SENDING;
                sendTasklist.pop();
                server.send_packet_to(client.get_uid(), 0, (enet_uint8* )st->_sendData.c_str(), st->_sendData.length(), ENET_PACKET_FLAG_RELIABLE);
                std::cout << "server: sendTask start send packet" << st->_ip<< ":"<< st->_port << std::endl;
                delete st;
                break;
            }
        }
    }

    void checkSendTaskTimeout(){

        while (!sendTasklist.empty()) {
            auto st = sendTasklist.front();
            if( st->isTimeout()){
                std::cout << "server: sendTask timeout " << st->_ip<< ":"<< st->_port << std::endl;
                sendTasklist.pop();
                delete st;
            }
        }
    }
    
        
    void run_server() {
        std::function<void(server_client& client)> on_client_connected = [&](server_client& client) {
            checkSendTaskOnConnected(client);
        };
        std::function<void(server_client& client)> on_client_disconnected = [&](server_client& client) {
            std::cout << "server: on_client_disconnected" << client._ip<< ":"<< client._port << std::endl;
        };
        
        std::function<void(server_client& client, const enet_uint8* data, size_t data_size)> on_client_data_received = [&](server_client& client, const enet_uint8* data, size_t data_size) {
            string filename = "./" + client._ip;
            std::ofstream o(filename.c_str(), ofstream::app); 
            o.write((const char*)data, data_size);               
            flush(o);
            std::cout << "server: received packet from client" << client._ip<< ":"<< client._port << "; len="<< data_size << std::endl;

            // server.tryDisconnect(client.get_uid());
            //            server.send_packet_to(client.get_uid(), 0, data, data_size, ENET_PACKET_FLAG_RELIABLE);
            
        };


        while (server.is_listening()) {
            server.consume_events(
                                  on_client_connected,
                                  on_client_disconnected,
                                  on_client_data_received);

            checkSendTaskTimeout();
            if (s_exit) {
                server.stop_listening();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
public:
    EnetSocket(){
        enetpp::global_state::get().initialize();
        enetpp::server<server_client> server;
    }
    void bind(unsigned short port){
        unsigned int next_uid = 0;
        auto init_client_func = [&](server_client& client, const char* ip, unsigned short port) {
            client._uid = next_uid;
            client._ip = string(ip);
            client._port = port;
            next_uid++;
        };
        auto trace_handler = [&](const std::string& msg) {
            std::lock_guard<std::mutex> lock(s_cout_mutex);
            std::cout << "server: " << msg << std::endl;
        };
        server.set_trace_handler(trace_handler);

        server.start_listening(enetpp::server_listen_params<server_client>()
                               .set_max_client_count(128)
                               .set_channel_count(1)
                               .set_listen_port(port)
                               .set_initialize_client_function(init_client_func));
            server_thread = make_unique<std::thread>(&EnetSocket::run_server, this);
            
    }
    
    void close(){
        s_exit = true;
        server_thread->join();
        server_thread.release();

        enetpp::global_state::get().deinitialize();
    }
    void connect(std::string ip, unsigned short port){
        server.tryConnect(ip, port);
    }

    int write(char* buffer, size_t size, std::string ip, unsigned short port){

        SendTask* st = new SendTask;
        st->_ip = ip;
        st->_port = port;
        st->_sendData = string(buffer, size);
        st->_start = clock();
        st->_timeout = 5000;
        st->_status = STATUS_NONE;
        sendTasklist.push(st);
        server.tryConnect(ip, port);
        std::cout << "server: sendTask add" << st->_ip<< ":"<< st->_port << "; dataLen="<< size << std::endl;
    }

    // void start_ice(std::vector<pair> v;
    //         std::function<void(string ip, unsigned short port)> on_ice_succ){
    //     _on_ice_succ = on_ice_succ;

    // }

};

