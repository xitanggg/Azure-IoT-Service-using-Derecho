/**
 * @file hello_world.cpp
 *
 * hello_world.cpp creates one subgroup of type Foo (defined in sample_objects.h).
 * It requires at least 2 nodes to join the group and they are all part of
 * the Foo subgroup. (Can be easily configured to 3 nodes as well)
 *
 * Every node (identified by its node_id) sends back T2 (T2 = time Derecho
 * process group receives TCP request) whenever it receives a TCP client connection.
 *
 * The first node also increases the foo_state by 1 in its subgroup each time
 * it receives a TCP client connection.
 *
 * The second node also prints "Hello World" and prints the foo_state each time
 * it receives a TCP client connection.
 *
 * Acknowledgement:
 * simple_replicated_objects.cpp: https://github.com/Derecho-Project/derecho/blob/master/src/applications/demos/simple_replicated_objects.cpp
 * Socket TCP Server in C: https://www.geeksforgeeks.org/socket-programming-cc/
 * Print time format in C: https://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811
 *
 * Xitang Zhao 2019-06-02
 **/

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <derecho/conf/conf.hpp>
#include <derecho/core/derecho.hpp>
#include "sample_objects.hpp"

using derecho::ExternalCaller;
using derecho::Replicated;
using std::cout;
using std::endl;

#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
    //----------------------Begin of Derecho Setup------------------------------
    // Read configurations from the command line options as well as the default config file
    derecho::Conf::initialize(argc, argv);

    //Define subgroup membership using the default subgroup allocator function
    //Each Replicated type will have one subgroup and one shard, with two members in the shard
    // Use derecho::fixed_even_shards(1,3) for three members group
    derecho::SubgroupInfo subgroup_function {derecho::DefaultSubgroupAllocator({
        {std::type_index(typeid(Foo)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1,2))}
    })};

    //Each replicated type needs a factory; this can be used to supply constructor arguments
    //for the subgroup's initial state. These must take a PersistentRegistry* argument, but
    //in this case we ignore it because the replicated objects aren't persistent.
    auto foo_factory = [](persistent::PersistentRegistry*) { return std::make_unique<Foo>(-1); };

    derecho::Group<Foo> group(derecho::CallbackSet{}, subgroup_function, nullptr,
                                          std::vector<derecho::view_upcall_t>{},
                                          foo_factory);

    cout << "Finished constructing/joining Derecho Group" << endl;

    //Now have each node send some updates to the Replicated objects
    //The code must be different depending on which subgroup this node is in,
    //which we can determine based on which membership list it appears in
    uint32_t my_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
    std::vector<node_id_t> foo_members = group.get_subgroup_members<Foo>(0)[0];
    auto find_in_foo_results = std::find(foo_members.begin(), foo_members.end(), my_id);
    uint32_t rank_in_foo = std::distance(foo_members.begin(), find_in_foo_results);
    Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
    int foo_state = 0;
    //----------------------End of Derecho Setup------------------------------

    //-----------------Begin of Time & Socket Setup---------------------------
    // Define variables for time info
    struct tm* tm_info;
    struct timeval tv;
    int time_buffer_max_size = 40;
    char print_time_buffer[time_buffer_max_size];
    std::string T2_string;
    char T2_char[time_buffer_max_size];

    // Set up socket server for TCP connection
    int socket_file_descriptor, new_socket_file_descriptor;
    int port_num = 80;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);
    char socket_read_buffer[256];
    int opt = 1;
    int n;
    bzero((char *) &server_address, sizeof(server_address));
    bzero(socket_read_buffer,256);

    // Create socket descriptor
    socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_file_descriptor < 0) {
        perror("ERROR opening socket"); exit(EXIT_FAILURE);
    }

    // Set socket option (helps in reuse of address and port)
    if (setsockopt(socket_file_descriptor, SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("ERROR setting socket"); exit(EXIT_FAILURE);
    }

    // Set and bind socket to address and port number
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port_num);
    if (bind(socket_file_descriptor,
            (struct sockaddr *) &server_address,
            sizeof(server_address)) < 0){
        perror("ERROR on binding"); exit(EXIT_FAILURE);
    }

    // Put socket in listen mode
    listen(socket_file_descriptor, 16);

    // Enter while loop to wait for TCP connection and run Derecho
    while (true){
        bzero(socket_read_buffer,256);
        // Establish connection between client and server
        new_socket_file_descriptor = accept(socket_file_descriptor,
                                            (struct sockaddr *) &client_address,
                                            &client_address_len);
        if (new_socket_file_descriptor < 0){
            perror("ERROR on accept"); exit(EXIT_FAILURE);
        }

        // Get current time T2 (T2 = time Derecho process group receives TCP request)
        gettimeofday(&tv, NULL);
        tm_info = localtime(&tv.tv_sec);
        strftime(print_time_buffer, time_buffer_max_size, "T2:%Y-%m-%d_%H:%M:%S.", tm_info);
        T2_string = print_time_buffer+std::to_string(tv.tv_usec)+"_UTC-04:00";
        strcpy(T2_char, T2_string.c_str());
        //printf("%s\n", T2_char);

        // Ignore TCP connection from 168.63.129.16, Azure health probes from Azure load balancer
        if (strcmp(inet_ntoa(client_address.sin_addr), "168.63.129.16") == 0){
            //printf("Ignore this IP\n");
        }
        // Take all other TCP connection
        else{
            //printf("Client's ip address is: %s\n", inet_ntoa(client_address.sin_addr));

            // Read data sent by client
            n = read(new_socket_file_descriptor, socket_read_buffer, 255);
            if (n < 0) {
                perror("ERROR reading from socket"); exit(EXIT_FAILURE);
            }
            //printf("Client's message: %s\n",socket_read_buffer);

            // Send T2 (timestamps it receives the message) to client
            n = write(new_socket_file_descriptor, T2_char, time_buffer_max_size);
            if (n < 0) {
                perror("ERROR writing to socket"); exit(EXIT_FAILURE);
            }

            // Run Derecho group processing if "HelloWorld" message received
            if (strcmp(socket_read_buffer, "HelloWorld") == 0){
                if(rank_in_foo == 0) {
                    foo_state += 1;
                    cout << "Changing Foo's state to " << foo_state << endl;
                    derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(foo_state);
                    decltype(results)::ReplyMap& replies = results.get();
                    cout << "Got a reply map!" << endl;
                    for(auto& reply_pair : replies) {
                        cout << "Reply from node " << reply_pair.first << " was " << std::boolalpha << reply_pair.second.get() << endl;
                    }
                    // Reading Foo's state just to allow node 1's message to be delivered
                    foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
                }
                else if(rank_in_foo == 1) {
                    cout << "Reading Foo's state from the group" << endl;
                    derecho::rpc::QueryResults<int> foo_results = foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
                    for(auto& reply_pair : foo_results.get()) {
                        cout << "HelloWorld - Node " << reply_pair.first << " says the state is: " << reply_pair.second.get() << endl;
                    }
                }
                // Uncomment the following if set up as three members group
                /*
                else if(rank_in_foo == 2) {
                    cout << "Reading Foo's state from the group" << endl;
                    derecho::rpc::QueryResults<int> foo_results = foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
                    for(auto& reply_pair : foo_results.get()) {
                        cout << "Node " << reply_pair.first << " says the state is: " << reply_pair.second.get() << endl;
                    }
                }
                */
            }
        }
    // Loop back in a while loop for the next TCP connection
    }
    close(new_socket_file_descriptor);
    close(socket_file_descriptor);
}
