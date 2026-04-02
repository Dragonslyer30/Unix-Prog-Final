This is a small-scale chat server made for a Unix Programming Final 

The Chat server features include:
  • Commands such as /help, /msg, /list, /quit, and /chat.
  • Uses epoll to lower thread usage.
  • Runs on the port 8080

To run the server compile it using the command:
gpp server.cpp -o server

After compilation, run the server by using the command:
./server

Possible features for the future:
  • Permanent username storage.
  • Passwords and basic password encryption.
  • Friends list.
  • Easier way to use chat rooms. (Maybe toggle into chats using /chat instead of having to type it each time)
