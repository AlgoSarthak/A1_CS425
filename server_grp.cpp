#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 12345
#define BUFFER_SIZE 1024

unordered_map<int, string> clients; 
unordered_map<string, string> users; 
unordered_map<string, unordered_set<int>> groups; 
mutex clients_mutex, users_mutex, groups_mutex;

void load_users(){
    ifstream file("users.txt");
    string line;
    while(getline(file, line)){
        istringstream iss(line);
        string username, password;
        if(getline(iss, username, ':') && getline(iss, password)){
            username.erase(username.find_last_not_of(" \n\r\t")+1);
            username.erase(0,username.find_first_not_of(" \n\r\t"));
            password.erase(password.find_last_not_of(" \n\r\t")+1);
            password.erase(0,password.find_first_not_of(" \n\r\t"));
            users[username] = password;
        }
    }
}

void broadcast_message(const string& message){
    lock_guard<mutex> lock(clients_mutex);
    for(auto& client : clients){
        send(client.first, message.c_str(), message.size(), 0);
    }
}

void process_broadcast_message(int client_socket, const string& message){
    string broadcast_msg = message.substr(11);
    broadcast_message("[Broadcast from " + clients[client_socket] + "]: " + broadcast_msg);
}

bool authenticate(int client_socket){
    char buffer[BUFFER_SIZE];
    string username, password;

    send(client_socket, "Enter username: ",16,0);
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE,0);
    username = buffer;

    send(client_socket, "Enter password: ",16, 0);
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE,0);
    password = buffer;

    lock_guard<mutex> lock(users_mutex);
    if(users.find(username) != users.end() && users[username] == password){
        lock_guard<mutex> lock(clients_mutex);
        clients[client_socket] = username;
        return true;
    }
    return false;
}

void authenticate_client(int client_socket){
    if(!authenticate(client_socket)){
        send(client_socket, "Authentication failed.\n", 23, 0);
        close(client_socket);
        return;
    }

    string welcome_message = "Welcome to the chat server!\n";
    send(client_socket, welcome_message.c_str(), welcome_message.size(), 0);

    string join_message = clients[client_socket] + " has joined the chat.\n";
    broadcast_message(join_message);
}

void private_message(int client_socket, const string& username, const string& message){
    lock_guard<mutex> lock(clients_mutex);
    bool found = false;
    for(auto& client : clients){
        if(client.second == username){
            send(client.first, message.c_str(), message.size(), 0);
            found = true;
            break;
        }
    }
    if(!found){
        string error_msg = "Error: User " + username + " does not exist.\n";
        send(client_socket, error_msg.c_str(), error_msg.size(), 0);
    }
}

void process_private_message(int client_socket, const string& message){
    size_t space1 = message.find(' ');
    size_t space2 = message.find(' ', space1 + 1);
    if(space1 != string::npos && space2 != string::npos){
        string username = message.substr(space1 + 1, space2 - space1 - 1);
        string private_msg = message.substr(space2 + 1);
        private_message(client_socket, username, "[" + clients[client_socket] + "]: " + private_msg);
    }
}

void group_message(int client_socket, const string& group_name, const string& message){
    lock_guard<mutex> lock(groups_mutex);
    auto it  = groups.find(group_name);
    if(it != groups.end()){
        if(it->second.find(client_socket) != it->second.end()){
            for(int member_socket : it->second){
                if(member_socket != client_socket){
                    send(member_socket, message.c_str(), message.size(), 0);
                }
            }
        } 
        else{
            string error_msg = "Error: You are not a member of the group " + group_name + ".\n";
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        }
    }
    else{
        string error_msg = "Error: Group " + group_name + " does not exist.\n";
        send(client_socket, error_msg.c_str(), error_msg.size(), 0);
    }
}

void process_group_message(int client_socket, const string& message){
    size_t space1 = message.find(' ');
    size_t space2 = message.find(' ', space1 + 1);
    if(space1 != string::npos && space2 != string::npos){
        string group_name = message.substr(space1 + 1, space2 - space1 - 1);
        string group_msg = message.substr(space2 + 1);
        group_message(client_socket, group_name, "[Group " + group_name + "]: " + group_msg + "\n");
    }
}

void create_group(int client_socket, const string& message){
    size_t space_pos = message.find(' ');
    if(space_pos != string::npos){
        string group_name = message.substr(space_pos + 1);
        lock_guard<mutex> lock(groups_mutex);
        if(groups.find(group_name) != groups.end()){
            string error_msg = "Error: Group " + group_name + " already exists.\n";
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        }
        else{
            groups[group_name] = unordered_set<int>();
            groups[group_name].insert(client_socket);
            string popup = "Group " + group_name + " created.\n";
            send(client_socket, popup.c_str(), popup.size(), 0);
        }
    }
}

void join_group(int client_socket, const string& message){
    size_t space_pos = message.find(' ');
    if(space_pos != string::npos){
        string group_name = message.substr(space_pos + 1);
        group_name.erase(group_name.find_last_not_of(" \n\r\t")+1);
        group_name.erase(0,group_name.find_first_not_of(" \n\r\t"));
        lock_guard<mutex> lock(groups_mutex);
        if(groups.find(group_name) != groups.end()){
            groups[group_name].insert(client_socket);
            string popup = "You joined the group " + group_name + ".\n";
            send(client_socket, popup.c_str(), popup.size(), 0);
        }
        else{
            string error_msg = "Error: Group " + group_name + " does not exist.\n";
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        }
    }
}

void leave_group(int client_socket, const string& message){
    size_t space_pos = message.find(' ');
    if(space_pos != string::npos){
        string group_name = message.substr(space_pos + 1);
        group_name.erase(group_name.find_last_not_of(" \n\r\t")+1);
        group_name.erase(0,group_name.find_first_not_of(" \n\r\t"));
        lock_guard<mutex> lock(groups_mutex);
        auto it = groups.find(group_name);
        if(it != groups.end()){
            if(it->second.find(client_socket) != it->second.end()){
                it->second.erase(client_socket);
                string popup = "You left the group " + group_name + ".\n";
                send(client_socket, popup.c_str(), popup.size(), 0);
            } 
            else{
                string error_msg = "Error: You are not a member of the group " + group_name + ".\n";
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
            }
        }
        else{
            string error_msg = "Error: Group " + group_name + " does not exist.\n";
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        }
    }
}

void disconnect_client(int client_socket){
    string leave_message = clients[client_socket] + " has left the chat.\n";
    broadcast_message(leave_message);
    {
        lock_guard<mutex> lock(groups_mutex);
        for(auto& group : groups) group.second.erase(client_socket);
    }
    {
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(client_socket);
    }
    close(client_socket);
}

void handle_client_command(int client_socket, const string& message){
    if(message.compare(0, 4, "/msg") == 0) process_private_message(client_socket, message);
    else if(message.compare(0, 10, "/broadcast") == 0) process_broadcast_message(client_socket, message);
    else if(message.compare(0, 13, "/create_group") == 0) create_group(client_socket, message);
    else if(message.compare(0, 11, "/join_group") == 0) join_group(client_socket, message);
    else if(message.compare(0, 12, "/leave_group") == 0) leave_group(client_socket, message);
    else if(message.compare(0, 10, "/group_msg") == 0) process_group_message(client_socket, message);
    else send(client_socket, "Error: Enter the correct prompt.\n", 32, 0);
}

void handle_client(int client_socket){
    authenticate_client(client_socket);

    char buffer[BUFFER_SIZE];
    while(true){
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if(bytes_received <= 0){
            break;
        }
        string message(buffer);
        handle_client_command(client_socket, message);
    }
    disconnect_client(client_socket);
}

void setup_server(int& server_socket){
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        perror("Error creating socket.");
        exit(1);
    }

    int yes=1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
        perror("Error setting socket options.");
        exit(1);
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0){
        perror("Error binding socket.");
        exit(1);
    }

    if(listen(server_socket, 100000) < 0){
        perror("Error listening on socket.");
        exit(1);
    }
    cout<<"Server is listening on port "<<PORT<<endl;
}

int main(){
    load_users();

    int server_socket;
    setup_server(server_socket);

    while(true){
        sockaddr_in client_address{};
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if(client_socket < 0){
            perror("Error accepting connection.");
            continue;
        }
        thread(handle_client, client_socket).detach();
    }
    close(server_socket);
    return 0;
}