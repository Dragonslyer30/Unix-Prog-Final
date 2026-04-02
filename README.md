This is a small-scale chat server made for a Unix Programming Final



The Chat server features include:

* Commands such as /help, /msg, /list, /quit, and /chat.
* Uses epoll to lower thread usage.
* Runs on the port 8080
* &#x20;Permanent username storage.
* &#x20;Passwords and basic password encryption.



To run the server compile it using the command:

g++ server.cpp userInfo.cpp encryption.cpp -o server



After compilation, run the server by using the command:

./server



Possible features for the future:

* &#x20;Friends list.
* &#x20;Easier way to use chat rooms. (Maybe toggle into chats using /chat instead of having to type it each time)

