#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <locale.h>
#include <sstream>

using namespace std;

int sock_fd;

// -------------------- MESSAGE STRUCT --------------------
struct Message {
    enum Type { CHAT, SYSTEM, BANNER } type;
    string text;
};

vector<Message> chatMessages;

int scroll_offset = 0;

// WINDOWS
WINDOW* banner_win;
WINDOW* chat_win;
WINDOW* input_win;

int term_h, term_w;
int chat_h;

// -------------------- CENTER TEXT --------------------
void center_text(WINDOW* win, int row, const string& text) {
    int x = (term_w - text.size()) / 2;
    if (x < 1) x = 1;
    mvwprintw(win, row, x, "%s", text.c_str());
}

// -------------------- RENDER BANNER --------------------
void render_banner(const string& title) {
    werase(banner_win);

    // ASCII border
    wborder(banner_win, '|', '|', '-', '-', '+', '+', '+', '+');

    wattron(banner_win, A_BOLD);
    center_text(banner_win, 1, title);
    wattroff(banner_win, A_BOLD);

    wrefresh(banner_win);
}

// -------------------- RENDER CHAT --------------------
void render_chat() {
    werase(chat_win);

    int visible = chat_h;
    int total = chatMessages.size();

    int start = total - visible - scroll_offset;
    if (start < 0) start = 0;

    int end = total - scroll_offset;
    if (end < start) end = start;

    int row = 0;

    for (int i = start; i < end; i++) {
        const Message& m = chatMessages[i];

        if (row >= chat_h) break;

        switch (m.type) {
            case Message::SYSTEM:
                wattron(chat_win, A_DIM);
                mvwprintw(chat_win, row++, 1, "%s", m.text.c_str());
                wattroff(chat_win, A_DIM);
                break;

            case Message::CHAT:
            default:
                mvwprintw(chat_win, row++, 1, "%s", m.text.c_str());
                break;

            case Message::BANNER:
                // handled separately (top bar)
                break;
        }
    }

    wrefresh(chat_win);
}

// -------------------- PUSH MESSAGE --------------------
void push_message(string msg) {
    if (!msg.empty() && msg.back() == '\n')
        msg.pop_back();

    // Split by newline FIRST (critical fix for TCP chunking)
    stringstream ss(msg);
    string line;

    while (getline(ss, line)) {
        if (line.empty()) continue;

        // -------- BANNER --------
        if (line.rfind("[BANNER]", 0) == 0) {
            string title = line.substr(8);
            render_banner(title);
            continue;
        }

        // -------- SYSTEM --------
        if (line.rfind("[SYSTEM]", 0) == 0) {
            chatMessages.push_back({Message::SYSTEM, line.substr(8)});
            continue;
        }

        // -------- CHAT --------
        chatMessages.push_back({Message::CHAT, line});
    }

    render_chat();
}

// -------------------- RECEIVE THREAD --------------------
void receive_messages() {
    char buffer[1024];

    while (true) {
        int bytes = read(sock_fd, buffer, sizeof(buffer) - 1);

        if (bytes > 0) {
            buffer[bytes] = '\0';
            push_message(string(buffer));
        }
    }
}

// -------------------- MAIN --------------------
int main() {
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock_fd, (sockaddr*)&server, sizeof(server));

    // FIX WEIRD BORDER CHARACTERS
    setlocale(LC_ALL, "");

    // INIT NCURSES
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    getmaxyx(stdscr, term_h, term_w);

    // Layout:
    // [ banner (3) ]
    // [ chat   (?) ]
    // [ input  (3) ]

    int banner_h = 3;
    int input_h = 3;
    chat_h = term_h - banner_h - input_h;

    banner_win = newwin(banner_h, term_w, 0, 0);
    chat_win   = newwin(chat_h, term_w, banner_h, 0);
    input_win  = newwin(input_h, term_w, banner_h + chat_h, 0);

    // Draw static borders
    wborder(banner_win, '|', '|', '-', '-', '+', '+', '+', '+');
    wborder(input_win,  '|', '|', '-', '-', '+', '+', '+', '+');

    scrollok(chat_win, FALSE);

    wrefresh(banner_win);
    wrefresh(chat_win);
    wrefresh(input_win);

    thread recv_thread(receive_messages);
    recv_thread.detach();

    string input;

    while (true) {
        // -------- INPUT BOX --------
        werase(input_win);
        wborder(input_win, '|', '|', '-', '-', '+', '+', '+', '+');

        // clear inside line
        mvwhline(input_win, 1, 1, ' ', term_w - 2);

        // padded input (fixes weird left chars)
        mvwprintw(input_win, 1, 2, "> %s", input.c_str());

        wrefresh(input_win);

        int ch = wgetch(input_win);

        // SCROLL UP
        if (ch == KEY_UP) {
            if (scroll_offset < (int)chatMessages.size() - chat_h)
                scroll_offset++;
            render_chat();
        }

        // SCROLL DOWN
        else if (ch == KEY_DOWN) {
            if (scroll_offset > 0)
                scroll_offset--;
            render_chat();
        }

        // SEND MESSAGE
        else if (ch == '\n') {
            string msg = input + "\n";
            write(sock_fd, msg.c_str(), msg.size());
            input.clear();
        }

        // BACKSPACE
        else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!input.empty())
                input.pop_back();
        }

        // NORMAL CHAR
        else if (ch >= 32 && ch <= 126) {
            input.push_back(ch);
        }
    }

    endwin();
    close(sock_fd);
    return 0;
}