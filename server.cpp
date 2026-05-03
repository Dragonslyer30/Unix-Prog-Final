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
#include "userInfo.h"
#include <cstdlib>
#include "chatStorage.h"

using namespace std;

string banner(const string& s) {
    const int width = 79; 

    int padding = (width - s.length()) / 2;

    if (padding < 0) padding = 0;

    string spaces(padding, ' ');

    string result =
        "===============================================================================\n" +
        spaces + s + "\n" +
        "===============================================================================\n";

    return result;
}

// Broadcast helper
void broadcast(const vector<int>& clients, const string& msg, int exclude_fd = -1) {
    for (int client : clients) {
        if (client != exclude_fd) {
            write(client, msg.c_str(), msg.size());
        }
    }
}

enum class allowedWords {admin};
void permissions(const int a, allowedWords){

}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(server_fd, SOMAXCONN);

    int epfd = epoll_create1(0);
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    vector<int> clients;
    unordered_map<int, string> selected_chat;
    vector<int> broadcast_chat;
    epoll_event events[10];

    unordered_map<int, string> usernames;
    unordered_map<string, int> name_to_fd;
    unordered_map<string, vector<int>> chats;
    unordered_map<int, vector<string>> allowedChats;
    userInfo user_pass;

    chatStorage chatText;

    cout << "Server running on port 8080...\n";

    while (true) {
        int n = epoll_wait(epfd, events, 10, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // 🔹 NEW CONNECTION
            if (fd == server_fd) {

                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) continue;

                auto& members = chats["main"];

                if (find(members.begin(), members.end(), client_fd) == members.end()) {
                    members.push_back(client_fd);
                }

                allowedChats[client_fd].push_back("main");

                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);

                clients.push_back(client_fd);
                
                string msg = string("[BANNER]Login\n") + "Enter username and password:\n";
                write(client_fd, msg.c_str(), msg.size());

                cout << "Client connected: " << client_fd << endl;
            }

            // 🔹 CLIENT DATA
            else {
                char buffer[1025];
                int bytes = read(fd, buffer, sizeof(buffer) - 1);

                if (bytes <= 0) {
                    // DISCONNECT
                    string leave_msg = "[SYSTEM]" + usernames[fd] + " left the chat\n";

                    string chat = selected_chat[fd];

                    if (chats.find(chat) != chats.end()) {
                        broadcast(chats[chat], leave_msg, fd);
                    }

                    string old_name = usernames[fd];
                    usernames.erase(fd);
                    name_to_fd.erase(old_name);

                    for (auto& p : chats) {
                        auto& members = p.second;
                        members.erase(remove(members.begin(), members.end(), fd), members.end());
                    }

                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    clients.erase(remove(clients.begin(), clients.end(), fd), clients.end());

                    cout << "Client disconnected: " << fd << endl;
                    continue;
                }

                buffer[bytes] = '\0';
                string message(buffer);

                // remove newline
                message.erase(remove(message.begin(), message.end(), '\n'), message.end());
                message.erase(remove(message.begin(), message.end(), '\r'), message.end());

                stringstream ss(message);
                vector<string> msg;
                string temp;
                while (ss >> temp) msg.push_back(temp);

                // 🔐 LOGIN
                if (usernames.find(fd) == usernames.end()) {
                    if (msg.size() < 2) {
                        string err = "Enter username and password\n";
                        write(fd, err.c_str(), err.size());
                        continue;
                    }

                    string username = msg[0];
                    string password = msg[1];

                    if (username.find_first_of(" \t") != string::npos) {
                        string err = "Invalid username\n";
                        write(fd, err.c_str(), err.size());
                        continue;
                    }

                    if (name_to_fd.count(username)) {
                        string err = "Username already in use\n";
                        write(fd, err.c_str(), err.size());
                        continue;
                    }

                    string stored = user_pass.findInfo(username);

                    if (stored == "null") {
                        user_pass.new_user(username, password);
                    } else if (stored != password) {
                        string err = "Invalid password\n";
                        write(fd, err.c_str(), err.size());
                        continue;
                    }

                    usernames[fd] = username;
                    name_to_fd[username] = fd;
                    selected_chat[fd] = "main";


                    temp = "[BANNER]Main\n";
                    write(fd, temp.c_str(), temp.size());

                    string join_msg = "[SYSTEM]" + username + " joined the chat\n";
                    broadcast(chats["main"], join_msg);

                    continue;
                }

                // 🔹 COMMANDS
                if (!message.empty() && message[0] == '/') {
                    string temp = message + "\n";
                    write(fd, temp.c_str(), temp.size());
                    if (msg[0] == "/help") {
                        string help =
                            "[SYSTEM]/quit: leave the server\n"
                            "[SYSTEM]/list: lists user in the server\n"
                            "[SYSTEM]/msg <user> <msg>: sends a private message to user\n"
                            "[SYSTEM]/chat create <name>: creates a private chat room\n"
                            "[SYSTEM]/chat add <chat> <user>: allows user to access the chat room\n"
                            "[SYSTEM]/chat <chat>: switches chat room\n";
                        write(fd, help.c_str(), help.size());
                    }

                    else if (msg[0] == "/quit") {
                        for (auto& pair : chats) {
                                auto& members = pair.second;
                                members.erase(remove(members.begin(), members.end(), fd), members.end());
                            }
                            string leave_msg = "[SYSTEM]" + usernames[fd] + " left the chat\n";
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

                    else if (msg[0] == "/list") {
                        string list = "Users:\n";
                        for (auto& p : usernames)
                            list += p.second + "\n";
                        write(fd, list.c_str(), list.size());
                    }

                    else if (msg[0] == "/msg" && msg.size() >= 3) {
                        string target = msg[1];

                        if (!name_to_fd.count(target)) {
                            string err = "[SYSTEM]User not found\n";
                            write(fd, err.c_str(), err.size());
                            continue;
                        }

                        string text;
                        for (int i = 2; i < msg.size(); i++)
                            text += msg[i] + " ";

                        string out = "(Private) " + usernames[fd] + ": " + text + "\n";
                        write(name_to_fd[target], out.c_str(), out.size());
                    }

                    else if (msg[0] == "/chat") {

                        if (msg.size() >= 3 && msg[1] == "create") {
                            auto& members = chats[msg[2]];
                            if (find(members.begin(), members.end(), fd) == members.end()) {
                                allowedChats[fd].push_back(msg[2]);
                            }
                            string m = "Chat created\n";
                            write(fd, m.c_str(), m.size());
                        }

                        else if (msg.size() >= 4 && msg[1] == "add" && msg[2] != "main") {
                            string chat = msg[2];
                            string user = msg[3];

                            if (!name_to_fd.count(user)) {
                                string err = "[SYSTEM]User not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            auto it = allowedChats.find(fd);

                            if (it == allowedChats.end() ||
                                find(it->second.begin(), it->second.end(), chat) == it->second.end()) {
                                string err = "[SYSTEM]You do not have permission to use this command\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            auto& chats = allowedChats[name_to_fd[user]];
                            if (find(chats.begin(), chats.end(), chat) == chats.end()) {
                                chats.push_back(chat);
                            }
                        }

                        else if (msg.size() == 2) {
                            string chat = msg[1];

                            if (chats.find(chat) == chats.end()) {
                                string err = "[SYSTEM]Chat not found\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Check permissions
                            auto it = allowedChats.find(fd);
                            if (it == allowedChats.end() || find(it->second.begin(), it->second.end(), chat) == it->second.end()) {
                                string err = "[SYSTEM]You do not have permission to view this chat\n";
                                write(fd, err.c_str(), err.size());
                                continue;
                            }

                            // Remove from old chat
                            auto& old_members = chats[selected_chat[fd]];
                            old_members.erase(remove(old_members.begin(), old_members.end(), fd), old_members.end());

                            // Join new chat
                            selected_chat[fd] = chat;
                            auto& new_members = chats[selected_chat[fd]];
                            if (find(new_members.begin(), new_members.end(), fd) == new_members.end()) {
                                new_members.push_back(fd);
                            }

                            temp = "[BANNER]" + selected_chat[fd] + "\n";
                            write(fd, temp.c_str(), temp.size());
                        }
                        continue;
                    }

                    else if (msg[0] == "/retrieve") {

                        if (msg.size() < 2) {
                            string err = "[SYSTEM]Usage: /retrieve <chat_name>\n";
                            write(fd, err.c_str(), err.size());
                            continue;
                        }

                        string chat_name = msg[1];

                        // Check if chat exists
                        if (chats.find(chat_name) == chats.end()) {
                            string err = "[SYSTEM]This chat does not exist\n";
                            write(fd, err.c_str(), err.size());
                            continue;
                        }

                        // Get chat history from file
                        string history = chatText.getChat(chat_name);

                        if (history.empty()) {
                            string msg = "[SYSTEM]No chat history available\n";
                            write(fd, msg.c_str(), msg.size());
                        } else {
                            write(fd, history.c_str(), history.size());
                        }
                    }

                    else {
                        string err = "[SYSTEM]Invalid command\n";
                        write(fd, err.c_str(), err.size());
                    }

                    continue;
                }

                // 🔹 NORMAL MESSAGE
                string out = usernames[fd] + ": " + message + "\n";
                if (chats.find(selected_chat[fd]) != chats.end()){
                    chatText.storeChat(selected_chat[fd], out);
                    broadcast(chats[selected_chat[fd]], out);
                } else {
                    string err = "[SYSTEM]Chat no longer exists\n";
                    write(fd, err.c_str(), err.size());
                }
            }
            
        }
    }
    close(server_fd);
    return 0;
}