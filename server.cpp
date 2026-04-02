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

using namespace std;

// 🔥 Broadcast helper
void broadcast(const vector<int>& clients, const string& msg, int exclude_fd = -1) {
    for (int client : clients) {
        if (client != exclude_fd) {
            write(client, msg.c_str(), msg.size());
        }
    }
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
    epoll_event events[10];

    unordered_map<int, string> usernames;
    unordered_map<string, int> name_to_fd;
    unordered_map<string, vector<int>> chats;
    userInfo user_pass;

    cout << "Server running on port 8080...\n";

    while (true) {
        int n = epoll_wait(epfd, events, 10, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            // 🔹 NEW CONNECTION
            if (fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) continue;

                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);

                clients.push_back(client_fd);

                string msg = "Use /help for commands\nEnter username and password:\n";
                write(client_fd, msg.c_str(), msg.size());

                cout << "Client connected: " << client_fd << endl;
            }

            // 🔹 CLIENT DATA
            else {
                char buffer[1025];
                int bytes = read(fd, buffer, sizeof(buffer) - 1);

                if (bytes <= 0) {
                    // DISCONNECT
                    string leave_msg = usernames[fd] + " left the chat\n";

                    broadcast(clients, leave_msg, fd);

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

                    string join_msg = username + " joined the chat\n";
                    broadcast(clients, join_msg);

                    continue;
                }

                // 🔹 COMMANDS
                if (!message.empty() && message[0] == '/') {

                    if (msg[0] == "/help") {
                        string help =
                            "/quit\n/name <name>\n/list\n/msg <user> <msg>\n"
                            "/chat create <name>\n/chat add <chat> <user>\n/chat <chat> <msg>\n";
                        write(fd, help.c_str(), help.size());
                    }

                    else if (msg[0] == "/quit") {
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

                    else if (msg[0] == "/list") {
                        string list = "Users:\n";
                        for (auto& p : usernames)
                            list += p.second + "\n";
                        write(fd, list.c_str(), list.size());
                    }

                    else if (msg[0] == "/name" && msg.size() >= 2) {
                        string new_name = msg[1];

                        if (name_to_fd.count(new_name)) {
                            string err = "Name taken\n";
                            write(fd, err.c_str(), err.size());
                            continue;
                        }

                        string old = usernames[fd];
                        name_to_fd.erase(old);

                        usernames[fd] = new_name;
                        name_to_fd[new_name] = fd;
                    }

                    else if (msg[0] == "/msg" && msg.size() >= 3) {
                        string target = msg[1];

                        if (!name_to_fd.count(target)) {
                            string err = "User not found\n";
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
                            chats[msg[2]].push_back(fd);
                            string m = "Chat created\n";
                            write(fd, m.c_str(), m.size());
                        }

                        else if (msg.size() >= 4 && msg[1] == "add") {
                            string chat = msg[2];
                            string user = msg[3];

                            if (!name_to_fd.count(user)) continue;

                            chats[chat].push_back(name_to_fd[user]);
                        }

                        else if (msg.size() >= 3) {
                            string chat = msg[1];

                            if (!chats.count(chat)) continue;

                            string text;
                            for (int i = 2; i < msg.size(); i++)
                                text += msg[i] + " ";

                            string out = "(Chat " + chat + ") " + usernames[fd] + ": " + text + "\n";

                            for (int member : chats[chat])
                                if (member != fd)
                                    write(member, out.c_str(), out.size());
                        }
                    }

                    else {
                        string err = "Invalid command\n";
                        write(fd, err.c_str(), err.size());
                    }

                    continue;
                }

                // 🔹 NORMAL MESSAGE
                string out = usernames[fd] + ": " + message + "\n";
                broadcast(clients, out, fd);
            }
        }
    }

    close(server_fd);
    return 0;
}