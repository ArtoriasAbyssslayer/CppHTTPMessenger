# C++ Messenger

A simple terminal-based chat application in C++ that works on Windows and Linux.

## Build

### Windows (Visual Studio 2022)
```batch
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Linux
```bash
cd build
cmake ..
make
```

## Run

### Start Server
```batch
cd build/bin/Release
messenger_server.exe
```

### Start Client
```batch
cd build/bin/Release
messenger_client.exe
```

## Commands
- `/register [username] [password]`  - Create user account (password encrypted)
- `/login [username] [password]`  - Login to chatroom 
<br>

##### After Log In
- -----
- `/users` - List active users
- `/help` - Show commands
- `/quit` - Exit
- `/clear` - Clear screen

## Notes

- Server runs on port 8080
- Multiple clients can connect
- Users are created from client side with register command 
- Users are stored in users.dat file in the same directory as client executable 
- The users.dat contains the username and the hashed password 
- Terminal user interfaced remade so thread race coditions don't mess up with update display 