// Cross-Platform CPP Messenger Server with Authentication
#ifdef WINDOWS_BUILD
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    // DO NOT define _CRT_SECURE_NO_WARNINGS here; configure it in your build system instead.
#endif

#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <map>
#include <ctime>
#include <sstream>
#include <fstream>
#include <functional>

#ifdef WINDOWS_BUILD
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    // Don't #define close to closesocket -- that breaks std::ifstream::close() etc.
#else
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

using namespace std;

struct User {
    string username;
    string passwordHash;
};

struct Client {
    SOCKET socket;
    string username;
    string ipAddress;
    bool authenticated;
};

vector<Client> clients;
mutex clientsMutex;
map<string, User> users;
mutex usersMutex;
map<string, vector<string>> messageHistory;
mutex historyMutex;

const string USERS_FILE = "users.dat";

#ifdef WINDOWS_BUILD
bool initWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup failed: " << result << endl;
        return false;
    }
    return true;
}

void cleanupWinsock() {
    WSACleanup();
}
#endif

// Cross-platform socket close helper
inline void closeSocket(SOCKET s) {
#ifdef WINDOWS_BUILD
    closesocket(s);
#else
    ::close(s);
#endif
}

// Simple hash function (use a proper library like bcrypt in production)
string hashPassword(const string& password) {
    hash<string> hasher;
    size_t hashValue = hasher(password + "SALT_2024");
    stringstream ss;
    ss << hashValue;
    return ss.str();
}

void loadUsers() {
    lock_guard<mutex> lock(usersMutex);
    ifstream file(USERS_FILE);
    if (!file.is_open()) {
        cout << "[*] No existing users file found. Starting fresh." << endl;
        return;
    }
    
    string username, passwordHash;
    while (file >> username >> passwordHash) {
        users[username] = {username, passwordHash};
    }
    file.close();
    cout << "[*] Loaded " << users.size() << " users from database." << endl;
}

void saveUser(const string& username, const string& passwordHash) {
    lock_guard<mutex> lock(usersMutex);
    ofstream file(USERS_FILE, ios::app);
    if (file.is_open()) {
        file << username << " " << passwordHash << endl;
        file.close();
    }
}

bool userExists(const string& username) {
    lock_guard<mutex> lock(usersMutex);
    return users.find(username) != users.end();
}

bool verifyPassword(const string& username, const string& password) {
    lock_guard<mutex> lock(usersMutex);
    if (users.find(username) == users.end()) {
        return false;
    }
    return users[username].passwordHash == hashPassword(password);
}

void broadcastMessage(const string& message, SOCKET senderSocket) {
    lock_guard<mutex> lock(clientsMutex);
    for (const auto& client : clients) {
        if (client.socket != senderSocket && client.authenticated) {
            send(client.socket, message.c_str(), (int)message.length(), 0);
        }
    }
}

void sendToClient(SOCKET socket, const string& message) {
    send(socket, message.c_str(), (int)message.length(), 0);
}

string getCurrentTime() {
    time_t now = time(0);
    char buf[80];
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));
    return string(buf);
}

void handleClient(SOCKET clientSocket, sockaddr_in clientAddr) {
    char buffer[4096];
    string username;
    string ipAddr = inet_ntoa(clientAddr.sin_addr);
    bool authenticated = false;
    
    // Send authentication prompt
    sendToClient(clientSocket, "[SYSTEM] Welcome! Commands: /login username password OR /register username password");
    
    // Authentication loop
    while (!authenticated) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            closeSocket(clientSocket);
            return;
        }
        
        string command(buffer);
        stringstream ss(command);
        string action, user, pass;
        ss >> action >> user >> pass;
        
        if (action == "/register") {
            if (user.empty() || pass.empty()) {
                sendToClient(clientSocket, "[ERROR] Usage: /register username password");
                continue;
            }
            
            if (userExists(user)) {
                sendToClient(clientSocket, "[ERROR] Username already exists!");
                continue;
            }
            
            string hashedPass = hashPassword(pass);
            users[user] = {user, hashedPass};
            saveUser(user, hashedPass);
            sendToClient(clientSocket, "[SUCCESS] Registration successful! Now use /login username password");
            cout << "[+] New user registered: " << user << endl;
            
        } else if (action == "/login") {
            if (user.empty() || pass.empty()) {
                sendToClient(clientSocket, "[ERROR] Usage: /login username password");
                continue;
            }
            
            if (!userExists(user)) {
                sendToClient(clientSocket, "[ERROR] Username not found! Use /register first.");
                continue;
            }
            
            if (!verifyPassword(user, pass)) {
                sendToClient(clientSocket, "[ERROR] Invalid password!");
                continue;
            }
            
            // Check if user already logged in
            {
                lock_guard<mutex> lock(clientsMutex);
                bool alreadyLoggedIn = false;
                for (const auto& client : clients) {
                    if (client.username == user && client.authenticated) {
                        alreadyLoggedIn = true;
                        break;
                    }
                }
                if (alreadyLoggedIn) {
                    sendToClient(clientSocket, "[ERROR] User already logged in!");
                    continue;
                }
            }
            
            username = user;
            authenticated = true;
            sendToClient(clientSocket, "[SUCCESS] Login successful! Welcome to the chat!");
            
        } else {
            sendToClient(clientSocket, "[ERROR] Unknown command. Use /login or /register");
        }
    }
    
    // Add authenticated client to list
    {
        lock_guard<mutex> lock(clientsMutex);
        clients.push_back({clientSocket, username, ipAddr, true});
        cout << "\n[+] " << username << " logged in from " << ipAddr << endl;
        cout << "[*] Active users: " << clients.size() << endl;
    }
    
    // Notify all clients
    string joinMsg = "[SYSTEM] " + username + " joined the chat";
    broadcastMessage(joinMsg, clientSocket);
    
    // Send welcome message
    string welcome = "[SYSTEM] Type /help for commands";
    sendToClient(clientSocket, welcome);
    
    // Main message loop
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        
        if (bytesReceived <= 0) {
            break;
        }
        
        string message(buffer);
        
        if (message == "/quit") {
            break;
        } else if (message == "/users") {
            lock_guard<mutex> lock(clientsMutex);
            stringstream ss;
            ss << clients.size();
            string userList = "\n[SYSTEM] === Active Users (" + ss.str() + ") ===\n";
            for (const auto& c : clients) {
                if (c.authenticated) {
                    userList += "[SYSTEM] - " + c.username;
                    if (c.socket == clientSocket) userList += " (you)";
                    userList += "\n";
                }
            }
            sendToClient(clientSocket, userList);
        } else if (message == "/help") {
            string help = "\n[SYSTEM] === Commands ===\n";
            help += "[SYSTEM] /users - List all users\n";
            help += "[SYSTEM] /help - Show this help\n";
            help += "[SYSTEM] /quit - Leave chat\n";
            sendToClient(clientSocket, help);
        } else {
            string timestamp = getCurrentTime();
            string fullMessage = "[" + timestamp + "] " + username + ": " + message;
            
            // Store in history
            {
                lock_guard<mutex> lock(historyMutex);
                messageHistory["global"].push_back(fullMessage);
                if (messageHistory["global"].size() > 100) {
                    messageHistory["global"].erase(messageHistory["global"].begin());
                }
            }
            
            // Broadcast to all authenticated clients
            broadcastMessage(fullMessage, (SOCKET)-1);
            
            // Log to server console
            cout << fullMessage << endl;
        }
    }
    
    // Client disconnected
    {
        lock_guard<mutex> lock(clientsMutex);
        clients.erase(
            remove_if(clients.begin(), clients.end(),
                [clientSocket](const Client& c) { return c.socket == clientSocket; }),
            clients.end()
        );
        cout << "\n[-] " << username << " left the chat" << endl;
        cout << "[*] Active users: " << clients.size() << endl;
    }
    
    string leaveMsg = "[SYSTEM] " + username + " left the chat";
    broadcastMessage(leaveMsg, (SOCKET)-1);
    
    closeSocket(clientSocket);
}

int main() {
    cout << "+========================================+\n";
    cout << "|   C++ Messenger Server v2.0            |\n";
    cout << "|   With User Authentication             |\n";
#ifdef WINDOWS_BUILD
    cout << "|        Windows Build                   |\n";
#else
    cout << "|        Linux/Unix Build                |\n";
#endif
    cout << "+========================================+\n\n";
    
    // Load existing users
    loadUsers();
    
#ifdef WINDOWS_BUILD
    if (!initWinsock()) {
        return 1;
    }
#endif
    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Error creating socket!" << endl;
#ifdef WINDOWS_BUILD
        cleanupWinsock();
#endif
        return 1;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
if (::bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {    cerr << "Error binding socket!" << endl;
    closeSocket(serverSocket);
#ifdef WINDOWS_BUILD
    cleanupWinsock();
#endif
    return 1;
}
    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        cerr << "Error listening on socket!" << endl;
        closeSocket(serverSocket);
#ifdef WINDOWS_BUILD
        cleanupWinsock();
#endif
        return 1;
    }
    
    cout << "[*] Server started on port 8080" << endl;
    cout << "[*] Waiting for connections...\n" << endl;
    
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);        
        if (clientSocket == INVALID_SOCKET) {
            
            cerr << "Error accepting connection!" << endl;
            continue;
        }

        thread clientThread(handleClient, clientSocket, clientAddr);
        clientThread.detach();
    }

    
    closeSocket(serverSocket);
#ifdef WINDOWS_BUILD
    cleanupWinsock();
#endif
    return 0;
}
