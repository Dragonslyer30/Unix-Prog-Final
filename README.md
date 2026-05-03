This is a small-scale chat server made for a Unix Programming Final



The Chat server features include:

* Commands such as /help, /msg, /list, /quit, and /chat.
* Uses epoll to lower thread usage.
* Runs on the port 8080
* Permanent username storage.
* Passwords and basic password encryption.
* Chats can be switched between and you only see the messages from the chat you are currently in.
* Client code to make chat room easier to read as a user.
* permanent chat storage



To run the server, download the files and give the run and compile command execute permissions using chmod.

Then use the commands "compile" and "run" to run the main server code.



To connect to the server, open another terminal and this time navigate to the "client" folder.

Then give the "compile" and "run" files execute permissions using chmod.

Then you can run the client code using the commands "compile" and "run".



You will be asked for a username and password. These are stored and if you enter a username that hasn't been entered before it will create a new user with that username and whatever password you put. This username and password is stored even if the server is restarted.



Use the /help command for a list of possible commands.

