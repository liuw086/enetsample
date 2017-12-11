#include <iostream>
#include <string>
#include <random>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <utility>
#include "enetpp/server.h"
#include "enetpp/global_state.h"
#include "enetpp/trace_handler.h"
#include "rc_utils.h"

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
    unsigned _start;
    int _timeout;
    int _status;

public:
    SendTask(){
    }

    bool isTimeout(){
        unsigned now = milive::RCUtils::getCurrentTime();
        int duration = now - _start;
        return duration > _timeout;
    }
};

class server_client {
public:
	unsigned int _uid;
    std::string _ip;
    unsigned short _port;
    

public:
	server_client()
		: _uid(0) {
	}

	unsigned int get_uid() const {
		return _uid;
	}
};

class EnetSocket {
    
private:
    enetpp::server<server_client> server;
    std::unique_ptr<std::thread> server_thread;
    bool s_exit = false;
    std::mutex s_cout_mutex;
    vector<SendTask* > sendTasklist;

    vector<pair<string, unsigned short>> _ice_addr_list;
    unsigned _ice_start;
    int _ice_timeout;

private:

    std::function<void(string ip, unsigned short port)> _on_ice_succ;
    std::function<void()> _on_ice_timeout;
    std::function<void(string ip, unsigned short port, const char* data, size_t data_size)> _on_data_received;
    std::function<void(const std::string& msg)> trace_handler = [&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(s_cout_mutex);
        std::cout << milive::RCUtils::getCurrentTimeStr() << " server: " << msg << std::endl;
    };

    bool isIceTimeout(){
        unsigned now = milive::RCUtils::getCurrentTime();
        int duration = now - _ice_start;
        return duration > _ice_timeout;
    }

    void checkSendTaskOnConnected(server_client& client){
        if(sendTasklist.empty()){
            return;
        }

        SendTask* st = NULL;
        vector<SendTask* >::iterator iter1 = sendTasklist.begin();
        vector<SendTask* >::iterator end = sendTasklist.end();
        for (; iter1 != end;)
        {
            vector<SendTask* >::iterator cur = iter1++;
            st = (SendTask *)*cur;
            if (NULL != st && !st->isTimeout() && st->_ip == client._ip && st->_port == client._port)
            {
                st->_status = STATUS_SENDING;
                server.send_packet_to(client.get_uid(), 0, (enet_uint8* )st->_sendData.c_str(), st->_sendData.length(), ENET_PACKET_FLAG_RELIABLE);
                std::cout << "server: sendTask start send packet" << st->_ip<< ":"<< st->_port << "; len="<< st->_sendData.length() <<"; list="<< sendTasklist.size() << std::endl;
                sendTasklist.erase(cur);
                delete st;
            }
        }
        
    }

    void checkIceOnConnected(server_client& client){
        if(!isIceing()){
            return;
        }
        std::string ip = client._ip;
        unsigned port = client._port;
        bool is_succ = false;
        for(auto addr : _ice_addr_list){
            if(addr.first == ip && addr.second == port){
                is_succ = true;
                trace_handler("ice ok:" +ip +":"+to_string(port));
                if(_on_ice_succ != NULL){
                    _on_ice_succ(ip, port);
                }
                break;
            }
        }
    }

    void checkSendTaskAndIceTimeout(){
        if(isIceing() && isIceTimeout()){
            server.closeConnectingPeer();

            vector<pair<string, unsigned short>> empty;
            std::swap( _ice_addr_list, empty );
            trace_handler("server: ice timeout ");
            _ice_start = 0;
            if(_on_ice_timeout != NULL){
                _on_ice_timeout();
            }
        }

        if(sendTasklist.empty()){
            return;
        }

        SendTask* st = NULL;
        vector<SendTask* >::iterator iter1 = sendTasklist.begin();
        vector<SendTask* >::iterator end = sendTasklist.end();
        for (; iter1 != end;)
        {
            vector<SendTask* >::iterator cur = iter1++;
            st = (SendTask *)*cur;
            if (NULL != st && st->isTimeout())
            {
                std::cout << "server: sendTask timeout" << st->_ip<< ":"<< st->_port << "; list="<< sendTasklist.size() << std::endl;
                sendTasklist.erase(cur);
                delete st;
                server.closeConnectingPeer();
            }
        }
    }
    
        
    void run_server() {
        auto on_client_connected = [&](server_client& client) {
            std::cout << "server: OnConnected" << client._ip << ":"<< client._port <<"; uid="<< client.get_uid() <<  "; sendtasksize="<< sendTasklist.size() << std::endl;
            checkSendTaskOnConnected(client);
            checkIceOnConnected(client);
        };

        auto on_client_disconnected = [&](server_client& client) {
            std::cout << "server: on_client_disconnected" << client._ip<< ":"<< client._port << std::endl;
        };
        
        auto on_client_data_received = [&](server_client& client, const enet_uint8* data, size_t data_size) {
            std::cout << "server: received packet from client" << client._ip<< ":"<< client._port << "; len="<< data_size << std::endl;
            if(_on_data_received != NULL){
                _on_data_received(client._ip, client._port, (const char*)data, data_size);
            }
        };


        while (server.is_listening()) {
            server.consume_events(
                                  on_client_connected,
                                  on_client_disconnected,
                                  on_client_data_received);

            checkSendTaskAndIceTimeout();
            if (s_exit) {
                server.stop_listening();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
public:
    EnetSocket(){
        enetpp::global_state::get().initialize();
        enetpp::server<server_client> server;
    }
    void bind(unsigned short port,
        std::function<void(string ip, unsigned short port, const char* data, size_t data_size)> on_data_received = NULL){

        _on_data_received = on_data_received;

        unsigned int next_uid = 0;
        auto init_client_func = [&](server_client& client, const char* ip, unsigned short port) {
            client._uid = next_uid;
            client._ip = string(ip);
            client._port = port;
            next_uid++;
        };
        // auto trace_handler = [&](const std::string& msg) {
        //     std::lock_guard<std::mutex> lock(s_cout_mutex);
        //     std::cout << "server: " << msg << std::endl;
        // };
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
        unsigned int uid;
        bool isConnected = server.isPeerConnected(ip, port, uid);
        if(isConnected){
            // st->_status = STATUS_SENDING;
            server.send_packet_to(uid, 0, (enet_uint8* )buffer, size, ENET_PACKET_FLAG_RELIABLE);
            std::cout << "server: write hasConnected Peer send packet" << ip<< ":"<< port << "; len="<<size << "; list="<< sendTasklist.size() << std::endl;
        }else{
            SendTask* st = new SendTask;
            st->_ip = ip;
            st->_port = port;
            st->_sendData = string(buffer, size);
            st->_start = milive::RCUtils::getCurrentTime();
            st->_timeout = 5000;
            st->_status = STATUS_NONE;
            sendTasklist.push_back(st);
            server.tryConnect(ip, port);
            std::cout << "server: sendTask add" << st->_ip<< ":"<< st->_port << "; dataLen="<< size << std::endl;
        }


        
        return size;
    }

    void start_ice(vector<std::pair<string, unsigned short>> addr_list, int timeout, 
        std::function<void(string ip, unsigned short port)> on_ice_succ, std::function<void()> on_ice_timeout = NULL){

        _ice_start = milive::RCUtils::getCurrentTime();
        _ice_timeout = timeout;
        _on_ice_succ = on_ice_succ;
        _on_ice_timeout = on_ice_timeout;
        bool isConnected = false;
        unsigned int uid;

        for(auto addr : addr_list){
            isConnected = server.isPeerConnected(addr.first, addr.second, uid);
            if(isConnected){
                trace_handler("start_ice has connected peer addr:" +addr.first +":"+to_string(addr.second) + "; uid="+to_string(uid));
                on_ice_succ(addr.first, addr.second);
            }else{
                std::cout << "server: start_ice addr: " << addr.first<< ":"<< addr.second << std::endl;
                server.tryConnect(addr.first, addr.second);
                _ice_addr_list.push_back(addr);
            }
        }

    }

    void stopIce(){
        if(isIceing()){
            trace_handler("stopIce ");

            server.closeConnectingPeer();
            vector<pair<string, unsigned short>> empty;
            std::swap( _ice_addr_list, empty);
            _ice_start = 0;
        }
    }

    bool isIceing(){
        return !_ice_addr_list.empty() || _ice_start > 0; ;
    }

    void clearPeers(){
        trace_handler("clearPeers ");
        server.clear_peers();
    }
};

