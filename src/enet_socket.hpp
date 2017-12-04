#include <iostream>
#include <string>
#include <random>
#include <stdio.h>
#include "enetpp/server.h"
#include "enetpp/global_state.h"

using namespace enetpp;

class server_client {
public:
	unsigned int _uid;

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
    
private:
    trace_handler trace_handler = [&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(s_cout_mutex);
        std::cout << "server: " << msg << std::endl;
    };
    
    std::function<void(server_client& client)> on_client_connected = [&](server_client& client) {
            std::string rip;
            unsigned port;
            server.get_peer_uid(client.get_uid(), rip, port);
            trace_handler("on_client_connected " + rip + ":" + std::to_string(port));
//            on_connected(rip, port);
        
//            std::string s = "1234567890";
//            enet_uint8* data = (enet_uint8* )s.c_str();
//            server.send_packet_to(client.get_uid(), 0, data, s.length(), ENET_PACKET_FLAG_RELIABLE);
        
    };
    std::function<void(unsigned int client_uid)> on_client_disconnected = [&](unsigned int client_uid) {
        std::string rip;
        unsigned port;
        server.get_peer_uid(client_uid, rip, port);
        trace_handler("on_client_disconnected " + rip + ":" + std::to_string(port));
//        on_disconnected(rip, port);
    };
    
    std::function<void(server_client& client, const enet_uint8* data, size_t data_size)> on_client_data_received = [&](server_client& client, const enet_uint8* data, size_t data_size) {
        std::string rip;
        unsigned port;
        server.get_peer_uid(client.get_uid(), rip, port);
        
        trace_handler("received packet from client : '" + std::string(reinterpret_cast<const char*>(data), data_size) +"("+std::to_string(data_size)+")" + "' from: " + rip + ":" + std::to_string(port));
        
        server.tryDisconnect(client.get_uid());
        //            server.send_packet_to(client.get_uid(), 0, data, data_size, ENET_PACKET_FLAG_RELIABLE);
        
    };
        
    void run_server() {
        
        while (server.is_listening()) {
            server.consume_events(
                                  on_client_connected,
                                  on_client_disconnected,
                                  on_client_data_received);
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
            next_uid++;
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

};

