#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unordered_map>


using namespace std;

int main() {
    // STEP 1: Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // STEP 2: Set server socket to non-blocking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // STEP 3: Define address
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    // STEP 4: Bind socket to address
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // STEP 5: Listen for connections
    listen(server_fd, SOMAXCONN);

    // STEP 6: Create epoll instance
    int epfd = epoll_create1(0);

    // STEP 7: Add server socket to epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    // Store connected clients
    vector<int> clients;

    // Buffer for events
    epoll_event events[10];

    cout << "Server running on port 8080...\n";

    unordered_map<int, string> usernames;
    unordered_map<string, int> name_to_fd;
    unordered_map<string, vector<int>> chats;

    // STEP 8: Event loop
    while (true) {
        int n = epoll_wait(epfd, events, 10, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // CASE 1: New connection
            if (fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) continue;

                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                epoll_event client_ev;
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);

                clients.push_back(client_fd);

                cout << "New client connected: " << client_fd << endl;

                string join_msg = "Use /help for a list of commands\n";
                write(client_fd, join_msg.c_str(), join_msg.size());
                
                string prompt = "Enter your username:\n";
                write(client_fd, prompt.c_str(), prompt.size());
            }
            // CASE 2: Client sent data
            else {
                char buffer[1024];
                int bytes = read(fd, buffer, sizeof(buffer));

                if (bytes == 0) {
                    for (auto& pair : chats) {
                        auto& members = pair.second;
                        members.erase(remove(members.begin(), members.end(), fd), members.end());
                    }
                    cout << "Client disconnected: " << fd << endl;

                    string leave_msg = usernames[fd] + " left the chat\n";
                    for (int client : clients) {
                        write(client, leave_msg.c_str(), leave_msg.size());
                    }

                    string old_name = usernames[fd];

                    usernames.erase(fd);
                    name_to_fd.erase(old_name);

                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);

                    clients.erase(remove(clients.begin(), clients.end(), fd), clients.end());
                }
                else if (bytes > 0) {
                    string message(buffer, bytes);
                    
                    if (usernames.find(fd) == usernames.end()) {

                        
                        message.erase(remove(message.begin(), message.end(), '\n'), message.end());
                        message.erase(remove(message.begin(), message.end(), '\r'), message.end());

                        
                        if (message.empty() || message.find_first_of(" \t") != string::npos) {
                            string invalid_name = "Usernames cannot be empty or contain spaces/tabs.\n";
                            write(fd, invalid_name.c_str(), invalid_name.size());
                            continue;
                        }

                        
                        if (name_to_fd.find(message) != name_to_fd.end()) {
                            string invalid_name = message + " is already in use\n";
                            write(fd, invalid_name.c_str(), invalid_name.size());
                            continue;
                        }

                        // If all checks pass, register the username
                        usernames[fd] = message;
                        name_to_fd[message] = fd;

                        // Announce the join to all clients
                        string join_msg = usernames[fd] + " joined the chat\n";
                        for (int client : clients) {
                            write(client, join_msg.c_str(), join_msg.size());
                        }
                        continue;
                    }

                    message.erase(remove(message.begin(), message.end(), '\n'), message.end());
                    message.erase(remove(message.begin(), message.end(), '\r'), message.end());

                    if (!message.empty() && message[0] == '/'){
                        
                        stringstream ss(message);
                        string cmd_msg;
                        vector<string> cmd;
                        while (ss >> cmd_msg){
                            cmd.push_back(cmd_msg);
                        }

                        if (cmd[0] == "/help"){
                            string help_msg = "List of commands:\n"
                            "/quit: quits the application.\n"
                            "/name <username>: changes your username\n"
                            "/list: lists the usernames of all users in the chat.\n"
                            "/msg <username> <message>: sends a private message to the user.\n"
                            "/chat <chat name> <message>: sends a message to a chat\n"
                            "/chat create <chat name>: creates a private chat room\n"
                            "/chat add <chat name> <user>: adds specified user into a chat\n";
                            write(fd, help_msg.c_str(), help_msg.size());
                        }
                        else if (cmd[0] == "/quit"){
                            for (auto& pair : chats) {
                                auto& members = pair.second;
                                members.erase(remove(members.begin(), members.end(), fd), members.end());
                            }
                            string leave_msg = usernames[fd] + " left the chat\n";
                            for (int client : clients){
                                if (client != fd){
                                    write(client, leave_msg.c_str(), leave_msg.size());
                                }
                            }
                            string old_name = usernames[fd];
                            usernames.erase(fd);
                            name_to_fd.erase(old_name);
                            close(fd);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                            clients.erase(remove(clients.begin(), clients.end(), fd), clients.end());
                            continue;
                        }
                        else if (cmd[0] == "/name") {
                            if (cmd.size() < 2) {
                                string msg = "Usage: /name <newname>\n";
                                write(fd, msg.c_str(), msg.size());
                                continue;
                            }

                            string new_name = cmd[1];

                            if (name_to_fd.find(new_name) != name_to_fd.end()) {
                                string err = "Username already taken\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            string old_name = usernames[fd];
                            name_to_fd.erase(old_name);

                            usernames[fd] = new_name;
                            name_to_fd[new_name] = fd;
                        }
                        else if (cmd[0] == "/list"){
                            string list_msg = "Connected users:\n";
                            for (const auto& pair : usernames) {
                                list_msg += pair.second + "\n";
                            }
                            write(fd, list_msg.c_str(), list_msg.size());
                        }
                        else if (cmd[0] == "/msg"){
                            if (cmd.size() < 3) {
                                string err = "Usage: /msg <username> <message>\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }
                            
                            string target_name = cmd[1];
                            cout << target_name << endl;
                            if (name_to_fd.find(target_name) == name_to_fd.end()) {
                                string err = "User not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }
                            string new_msg;
                            for (int i = 2; i < cmd.size(); i++){
                                new_msg += cmd[i] + " ";
                            }
                            
                            string full_msg = "(Private) " + usernames[fd] + ": " + new_msg + "\n";

                            int target_fd = name_to_fd[target_name];
                            write(target_fd, full_msg.c_str(), full_msg.size());
                        }
                        else if (cmd[0] == "/chat") {

                        if (cmd.size() < 2) {
                            string err = "Invalid /chat usage\n";
                            write(fd, err.c_str(), err.size());
                            continue;
                        }

                        if (cmd[1] == "create") {
                            if (cmd.size() < 3) {
                                string err = "Usage: /chat create <name>\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            string chat_name = cmd[2];

                            // Check if chat already exists
                            if (chats.find(chat_name) != chats.end()) {
                                string err = "Chat already exists\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Create chat and add creator
                            chats[chat_name].push_back(fd);

                            string msg = "Chat '" + chat_name + "' created\n";
                            write(fd, msg.c_str(), msg.size());
                        }
                        else if (cmd[1] == "add") {
                            if (cmd.size() < 4) {
                                string err = "Usage: /chat add <chat> <user>\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            string chat_name = cmd[2];
                            string target_user = cmd[3];

                            // Check chat exists
                            if (chats.find(chat_name) == chats.end()) {
                                string err = "Chat not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Check user exists
                            if (name_to_fd.find(target_user) == name_to_fd.end()) {
                                string err = "User not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            int target_fd = name_to_fd[target_user];

                            // Prevent duplicates
                            auto& members = chats[chat_name];
                            if (find(members.begin(), members.end(), target_fd) != members.end()) {
                                string err = "User already in chat\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Add user
                            members.push_back(target_fd);

                            string msg = target_user + " added to chat '" + chat_name + "'\n";
                            write(fd, msg.c_str(), msg.size());
                        }
                        else if (cmd[1] == "leave") {
                            if (cmd.size() < 3) {
                                string err = "Usage: /chat leave <name>\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            string chat_name = cmd[2];

                            if (chats.find(chat_name) == chats.end()) {
                                string err = "Chat not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            auto& members = chats[chat_name];
                            members.erase(remove(members.begin(), members.end(), fd), members.end());

                            string msg = "You left chat '" + chat_name + "'\n";
                            write(fd, msg.c_str(), msg.size());
                        }
                        else {
                            if (cmd.size() < 3) {
                                string err = "Usage: /chat <name> <message>\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            string chat_name = cmd[1];

                            // Check chat exists
                            if (chats.find(chat_name) == chats.end()) {
                                string err = "Chat not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Check sender is in chat
                            auto& members = chats[chat_name];
                            if (find(members.begin(), members.end(), fd) == members.end()) {
                                string err = "You are not in this chat\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Rebuild message
                            string new_msg;
                            for (int i = 2; i < cmd.size(); i++) {
                                new_msg += cmd[i] + " ";
                            }

                            // Format message
                            string full_msg = "(Chat " + chat_name + ") " + usernames[fd] + ": " + new_msg + "\n";

                            // Send to all members
                            for (int member_fd : members) {
                                if (member_fd != fd) {
                                    write(member_fd, full_msg.c_str(), full_msg.size());
                                }
                            }
                        }
                    }
                        else{
                            string invalid_cmd = "Invalid command, use /help for a list of commands.\n";
                            write(fd, invalid_cmd.c_str(), invalid_cmd.size());
                        }
                        continue;
                    }

                    cout << "Message from " << fd << ": " << message;

                    for (int client : clients) {
                        if (client != fd) {
                            string full_msg = usernames[fd] + ": " + message + "\n";
                            write(client, full_msg.c_str(), full_msg.size());
                        }
                    }
                }
            }
        }
    }
    close(server_fd);
    return 0;
}

