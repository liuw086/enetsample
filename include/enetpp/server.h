#ifndef ENETPP_SERVER_H_
#define ENETPP_SERVER_H_

#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>

#include "server_listen_params.h"
#include "server_queued_packet.h"
#include "server_event.h"
#include "global_state.h"
#include "set_current_thread_name.h"
#include "trace_handler.h"
#include "enet/time.h"

using namespace std;

namespace enetpp {
    

	template<typename ClientT>
	class server {
	public:
		using event_type = server_event<ClientT>;
		using listen_params_type = server_listen_params<ClientT>;
		using client_ptr_vector = std::vector<ClientT*>;

	private:
		trace_handler _trace_handler;

		bool _should_exit_thread;
		std::unique_ptr<std::thread> _thread;

		//mapping of uid to peer so that sending packets to specific peers is safe.
		std::unordered_map<unsigned int, ENetPeer*> _thread_peer_map;
		vector<ENetPeer*> _connecting_peers;

		client_ptr_vector _connected_clients;
		std::mutex _connected_clients_mutex;

		std::queue<server_queued_packet> _packet_queue;
        
        std::queue<ENetAddress> _try_connect_queue;
        std::queue<unsigned int> _try_disconnect_queue;
        
		std::mutex _packet_queue_mutex;

		std::queue<event_type> _event_queue;
		std::queue<event_type> _event_queue_copy; //member variable instead of stack variable to prevent mallocs?
		std::mutex _event_queue_mutex;

		int _last_log_time;
		bool _is_close_connecting_peer;
		bool _is_clear_peers;

	public:
		server() 
			: _should_exit_thread(false) {
				_is_close_connecting_peer = false;
				_is_clear_peers = false;
		}

		~server() {
			//responsibility of owners to make sure stop_listening is always called. not calling stop_listening() in destructor due to
			//trace_handler side effects.
			assert(_thread == nullptr);
			assert(_packet_queue.empty());
			assert(_event_queue.empty());
			assert(_event_queue_copy.empty());
			assert(_connected_clients.empty());
		}
        
		void set_trace_handler(trace_handler handler) {
			assert(!is_listening()); //must be set before any threads started to be safe
			_trace_handler = handler;
		}

		bool is_listening() const {
			return (_thread != nullptr);
		}

		void start_listening(const listen_params_type& params) {
			assert(global_state::get().is_initialized());
			assert(!is_listening());
			assert(params._max_client_count > 0);
			assert(params._channel_count > 0);
			assert(params._listen_port != 0);
			assert(params._initialize_client_function != nullptr);

			trace("listening on port " + std::to_string(params._listen_port));

			_should_exit_thread = false;
			_thread = make_unique<std::thread>(&server::run_in_thread, this, params);
		}

		void stop_listening() {
			if (_thread != nullptr) {
				_should_exit_thread = true;
				_thread->join();
				_thread.release();
			}

			destroy_all_queued_packets();
			destroy_all_queued_events();
			delete_all_connected_clients();
		}

		void clear_peers() {
			_is_clear_peers = true;

			destroy_all_queued_packets();
			destroy_all_queued_events();
			delete_all_connected_clients();
		}

		void send_packet_to(unsigned int client_id, enet_uint8 channel_id, const enet_uint8* data, size_t data_size, enet_uint32 flags) {
			assert(is_listening());
			if (_thread != nullptr) {
				std::lock_guard<std::mutex> lock(_packet_queue_mutex);
				auto packet = enet_packet_create(data, data_size, flags);
				_packet_queue.emplace(channel_id, packet, client_id);
			}
		}

		void send_packet_to_all_if(enet_uint8 channel_id, const enet_uint8* data, size_t data_size, enet_uint32 flags, std::function<bool(const ClientT& client)> predicate) {
			assert(is_listening());
			if (_thread != nullptr) {
				std::lock_guard<std::mutex> lock(_packet_queue_mutex);
				auto packet = enet_packet_create(data, data_size, flags);
				for (auto c : _connected_clients) {
					if (predicate(*c)) {
						_packet_queue.emplace(channel_id, packet, c->get_uid());
					}
				}
			}
		}

		void consume_events(
			std::function<void(ClientT& client)> on_client_connected,
			std::function<void(ClientT& client)> on_client_disconnected,
			std::function<void(ClientT& client, const enet_uint8* data, size_t data_size)> on_client_data_received) {

			if (!_event_queue.empty()) {

				{
					std::lock_guard<std::mutex> lock(_event_queue_mutex);
					assert(_event_queue_copy.empty());
					_event_queue_copy = _event_queue;
					std::queue<event_type> empty;
					std::swap( _event_queue, empty );
				}

				while (!_event_queue_copy.empty()) {
					auto& e = _event_queue_copy.front();
					switch (e._event_type) {
						case ENET_EVENT_TYPE_CONNECT: {
							{
								std::lock_guard<std::mutex> lock(_connected_clients_mutex);
								_connected_clients.push_back(e._client);
							}
							on_client_connected(*e._client);
							break;
						}

						case ENET_EVENT_TYPE_DISCONNECT: {
							
                            auto iter = std::find(_connected_clients.begin(), _connected_clients.end(), e._client);
							assert(iter != _connected_clients.end());
							// unsigned int client_id = e._client->get_uid();
                            on_client_disconnected(*e._client);
                            {
								std::lock_guard<std::mutex> lock(_connected_clients_mutex);
								_connected_clients.erase(iter);
							}
                            
							delete e._client;
							
							break;
						}

						case ENET_EVENT_TYPE_RECEIVE: {
							on_client_data_received(*e._client, e._packet->data, e._packet->dataLength);
							enet_packet_destroy(e._packet);
							break;
						}

						case ENET_EVENT_TYPE_NONE:
						default:
							assert(false);
							break;
					}
					_event_queue_copy.pop();
				}
			}
		}

		bool isPeerConnected(string ip, unsigned short port, unsigned int& uid){
			std::lock_guard<std::mutex> lock(_connected_clients_mutex);
			for (auto c : _connected_clients) {
				if(c->_ip == ip && c->_port == port){
					uid = c->get_uid();
					return  true;
				}
			}
			return false;
		}

		const client_ptr_vector& get_connected_clients() const {
			return _connected_clients;
		}
        
        void tryConnect(std::string serverAddr, unsigned short port){
            ENetAddress svraddr;
            enet_address_set_host(&svraddr, serverAddr.c_str());
            svraddr.port = port;
            _try_connect_queue.push(svraddr);
        }
        
        void tryDisconnect(unsigned int uid){
            _try_disconnect_queue.push(uid);
        }

        void closeConnectingPeer(){
        	_is_close_connecting_peer = true;
        		trace("closeConnectingPeer ;connected=" + std::to_string(_thread_peer_map.size())
	    			+ ";connecting=" + std::to_string(_connecting_peers.size()));
        }

	private:
		void run_in_thread(const listen_params_type& params) {
			set_current_thread_name("enetpp::server");

			auto address = params.make_listen_address();
			ENetHost* host = enet_host_create(
				&address,
				params._max_client_count,
				params._channel_count,
				params._incoming_bandwidth,
				params._outgoing_bandwidth);
			if (host == nullptr) {
				trace("enet_host_create failed");
            }else{
                trace("listen succ fd = " + std::to_string(host->socket));
            }

			while (host != nullptr) {
				if(_is_clear_peers){
					disconnect_all_peers_in_thread();
					_is_clear_peers = false;
				}
				if (_should_exit_thread) {
					disconnect_all_peers_in_thread();
					enet_host_destroy(host);
                    trace("close succ fd = " + std::to_string(host->socket));
					host = nullptr;
				}

				if (host != nullptr) {
                    connect_or_disconnect_packets_in_thread(host);
                    send_queued_packets_in_thread();
					capture_events_in_thread(params, host);
					checkPeers(host);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		}
		void checkPeers(ENetHost* server){
            ENetPeer* peer;
            int peerCount = 0;
            for (peer = server -> peers; peer < & server -> peers [server -> peerCount]; ++ peer)
            {
                if(peer -> state == ENET_PEER_STATE_CONNECTED || peer -> state == ENET_PEER_STATE_CONNECTION_SUCCEEDED){
                    peerCount++;
                    if(ENET_TIME_DIFFERENCE(enet_time_get(), peer->lastReceiveTime) > 10 * 1000){
                    	if(ENET_TIME_DIFFERENCE(enet_time_get(), _last_log_time) > 10 * 1000){
                    		trace("checkPeers timeout: sumOut=" + std::to_string(peer->totalSentData) +"; sumIn=" + 
                    			std::to_string(peer->incomingDataTotal) + ";connected=" + std::to_string(_thread_peer_map.size())
                    			+ ";connecting=" + std::to_string(_connecting_peers.size()));
                    		_last_log_time = enet_time_get();
                    	}
                        // enet_peer_reset(peer);
                    }
                }
            }

		}

		void disconnect_all_peers_in_thread() {
			for (auto iter : _thread_peer_map) {
				enet_peer_disconnect_now(iter.second, 0);
				iter.second->data = nullptr;
			}
			_thread_peer_map.clear();

			for (auto iter : _connecting_peers) {
				enet_peer_disconnect_now(iter, 0);
			}
			_connecting_peers.clear();
		}
        void connect_or_disconnect_packets_in_thread(ENetHost* host) {
        	if(_is_close_connecting_peer){
	        	_is_close_connecting_peer = false;
	        	for (auto iter : _connecting_peers) {
					enet_peer_disconnect_now(iter, 0);
				}
				_connecting_peers.clear();
            }

            if (!_try_connect_queue.empty()) {
                std::lock_guard<std::mutex> lock(_packet_queue_mutex);
                while (!_try_connect_queue.empty()) {
                    auto qp = _try_connect_queue.front();
                    _try_connect_queue.pop();
                    ENetPeer *connectingPeer=enet_host_connect(host, &qp, 3, 0);
                    if(connectingPeer == NULL)
                    {
                        trace("enet_peer_send failed");
                    }else{
                        char ip[256];
                        enet_address_get_host_ip(&qp,ip,256);
                        trace("try connect " + std::string(ip) +":" + std::to_string(qp.port));
                        _connecting_peers.push_back(connectingPeer);
                    }
                }
            }
            if (!_try_disconnect_queue.empty()) {
                std::lock_guard<std::mutex> lock(_packet_queue_mutex);
                while (!_try_disconnect_queue.empty()) {
                    auto uid = _try_disconnect_queue.front();
                    _try_disconnect_queue.pop();
                    auto pi = _thread_peer_map.find(uid);
                    if (pi != _thread_peer_map.end()) {
                        
                        ENetAddress remote = pi->second->address; //远程地址
                        char ip[256];
                        enet_address_get_host_ip(&remote,ip,256);
                        trace("now disconnect " + std::string(ip) +":" + std::to_string(remote.port) + ";uid="+ to_string(uid));
                        enet_peer_disconnect_now(pi->second, 0);

                        pi->second->data = nullptr;
                        _thread_peer_map.erase(pi);
                        
                    }
                }
            }
        }
		void send_queued_packets_in_thread() {
			if (!_packet_queue.empty()) {
				std::lock_guard<std::mutex> lock(_packet_queue_mutex);
				while (!_packet_queue.empty()) {
					auto qp = _packet_queue.front();
					_packet_queue.pop();

					auto pi = _thread_peer_map.find(qp._client_id);
					if (pi != _thread_peer_map.end()) {

						//enet_peer_send fails if state not connected. was getting random asserts on peers disconnecting and going into ENET_PEER_STATE_ZOMBIE.
						if (pi->second->state == ENET_PEER_STATE_CONNECTED) {

							if (enet_peer_send(pi->second, qp._channel_id, qp._packet) != 0) {
								trace("enet_peer_send failed");
							}

							if (qp._packet->referenceCount == 0) {
								enet_packet_destroy(qp._packet);
							}
						}
					}
				}
			}
		}

		void capture_events_in_thread(const listen_params_type& params, ENetHost* host) {
			//http://lists.cubik.org/pipermail/enet-discuss/2013-September/002240.html
			enet_host_service(host, 0, 3);

			ENetEvent e;
			while (enet_host_check_events(host, &e) > 0) {
				switch (e.type) {
				case ENET_EVENT_TYPE_CONNECT: {
					handle_connect_event_in_thread(params, e);
					break;
				}

				case ENET_EVENT_TYPE_RECEIVE: {
					handle_receive_event_in_thread(e);
					break;
				}

				case ENET_EVENT_TYPE_DISCONNECT: {
					handle_disconnect_event_in_thread(e);
					break;
				}

				case ENET_EVENT_TYPE_NONE:
				default:
					assert(false);
					break;
				}
			}
		}

		void handle_connect_event_in_thread(const listen_params_type& params, const ENetEvent& e) {
			enet_uint32 enet_timeout = static_cast<enet_uint32>(params._peer_timeout.count());
			enet_peer_timeout(e.peer, 0, enet_timeout, enet_timeout);
            enet_peer_ping_interval(e.peer, 10000);

			char peer_ip[256];
			enet_address_get_host_ip(&e.peer->address, peer_ip, 256);

			//!IMPORTANT! PeerData and it's UID must be created immediately in this worker thread. Otherwise
			//there is a chance the first few packets are received on the worker thread when the peer is not 
			//initialized with data causing them to be discarded.

			auto iter = std::find(_connecting_peers.begin(), _connecting_peers.end(), e.peer);
			if(iter != _connecting_peers.end()){
				_connecting_peers.erase(iter);
			}

			auto client = new ClientT();
			params._initialize_client_function(*client, peer_ip, e.peer->address.port);

			assert(e.peer->data == nullptr);
			e.peer->data = client;

			_thread_peer_map[client->get_uid()] = e.peer;

			{
				std::lock_guard<std::mutex> lock(_event_queue_mutex);
				_event_queue.emplace(ENET_EVENT_TYPE_CONNECT, 0, nullptr, client);
			}
		}

		void handle_disconnect_event_in_thread(const ENetEvent& e) {
			auto client = reinterpret_cast<ClientT*>(e.peer->data);
			if (client != nullptr) {
				auto iter = _thread_peer_map.find(client->get_uid());
				assert(iter != _thread_peer_map.end());
				assert(iter->second == e.peer);
				trace("handle_disconnect_event_in_thread : intransit" + std::to_string(e.peer->reliableDataInTransit) 
                    		+" ;sumOut=" + std::to_string(e.peer->totalSentData) +"; sumIn=" + std::to_string(e.peer->incomingDataTotal) + "; peersize=" +
                    		 std::to_string(_thread_peer_map.size()) + "; uid="+ std::to_string(client->get_uid()));

				e.peer->data = nullptr;
				_thread_peer_map.erase(iter);

				std::lock_guard<std::mutex> lock(_event_queue_mutex);
				_event_queue.emplace(ENET_EVENT_TYPE_DISCONNECT, 0, nullptr, client);
			}
		}

		void handle_receive_event_in_thread(const ENetEvent& e) {
			auto client = reinterpret_cast<ClientT*>(e.peer->data);
			if (client != nullptr) {
				std::lock_guard<std::mutex> lock(_event_queue_mutex);
				_event_queue.emplace(ENET_EVENT_TYPE_RECEIVE, e.channelID, e.packet, client);
			}
		}

		void destroy_all_queued_packets() {
			std::lock_guard<std::mutex> lock(_packet_queue_mutex);
			while (!_packet_queue.empty()) {
				enet_packet_destroy(_packet_queue.front()._packet);
				_packet_queue.pop();
			}
		}

		void destroy_all_queued_events() {
			std::lock_guard<std::mutex> lock(_event_queue_mutex);
			while (!_event_queue.empty()) {
				destroy_unhandled_event_data(_event_queue.front());
				_event_queue.pop();
			}
		}

		void delete_all_connected_clients() {
			std::lock_guard<std::mutex> lock(_connected_clients_mutex);
			for (auto c : _connected_clients) {
				delete c;
			}
			_connected_clients.clear();
		}

		void destroy_unhandled_event_data(event_type& e) {
			if (e._event_type == ENET_EVENT_TYPE_CONNECT) {
				delete e._client;
			}
			else if (e._event_type == ENET_EVENT_TYPE_RECEIVE) {
				enet_packet_destroy(e._packet);
			}
		}

		void trace(const std::string& s) {
			if (_trace_handler != nullptr) {
				_trace_handler(s);
			}
		}

	};

}

#endif
