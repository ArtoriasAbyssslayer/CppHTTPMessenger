// Cross-Platform Modern Messenger Client with Authentication
#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>

#ifdef WINDOWS_BUILD
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <conio.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define close closesocket
#else
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include <signal.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
#endif

using namespace std;

atomic<bool> running(true);
atomic<bool> authenticated(false);
SOCKET clientSocket;
string username;
mutex displayMutex;
int messageRow = 4;

// Terminal control functions
void clearScreen() {
#ifdef WINDOWS_BUILD
    system("cls");
#else
    cout << "\033[2J\033[1;1H";
#endif
}

void moveCursor(int row, int col) {
#ifdef WINDOWS_BUILD
    COORD coord;
    coord.X = (SHORT)(col - 1);
    coord.Y = (SHORT)(row - 1);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
#else
    cout << "\033[" << row << ";" << col << "H";
#endif
}

void setColor(const string& color) {
#ifdef WINDOWS_BUILD
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (color == "red") SetConsoleTextAttribute(hConsole, 12);
    else if (color == "green") SetConsoleTextAttribute(hConsole, 10);
    else if (color == "yellow") SetConsoleTextAttribute(hConsole, 14);
    else if (color == "blue") SetConsoleTextAttribute(hConsole, 9);
    else if (color == "magenta") SetConsoleTextAttribute(hConsole, 13);
    else if (color == "cyan") SetConsoleTextAttribute(hConsole, 11);
    else if (color == "white") SetConsoleTextAttribute(hConsole, 15);
    else if (color == "bold") SetConsoleTextAttribute(hConsole, 15);
    else if (color == "reset") SetConsoleTextAttribute(hConsole, 7);
#else
    if (color == "red") cout << "\033[31m";
    else if (color == "green") cout << "\033[32m";
    else if (color == "yellow") cout << "\033[33m";
    else if (color == "blue") cout << "\033[34m";
    else if (color == "magenta") cout << "\033[35m";
    else if (color == "cyan") cout << "\033[36m";
    else if (color == "white") cout << "\033[37m";
    else if (color == "bold") cout << "\033[1m";
    else if (color == "reset") cout << "\033[0m";
#endif
}

int getTerminalHeight() {
#ifdef WINDOWS_BUILD
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
#endif
}

int getTerminalWidth() {
#ifdef WINDOWS_BUILD
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
#endif
}

void drawHeader() {
    int width = getTerminalWidth();
    moveCursor(1, 1);
    setColor("cyan");
    setColor("bold");
    cout << string(width, '=');
    moveCursor(2, 1);
    string title = "  C++ MESSENGER  ";
    int padding = (width - (int)title.length()) / 2;
    cout << string(padding, ' ') << title;
    moveCursor(3, 1);
    cout << string(width, '=');
    setColor("reset");
}

void drawFooter() {
    int height = getTerminalHeight();
    int width = getTerminalWidth();
    
    moveCursor(height - 3, 1);
    setColor("cyan");
    cout << string(width, '-');
    setColor("reset");
    
    moveCursor(height - 2, 1);
    setColor("yellow");
    cout << "Commands: /help /users /quit" << string(width - 28, ' ');
    setColor("reset");
    
    moveCursor(height - 1, 1);
    setColor("green");
    cout << username << " > " << string(width - username.length() - 3, ' ');
    setColor("reset");
    cout.flush();
}

void drawUI() {
    clearScreen();
    drawHeader();
    drawFooter();
    moveCursor(4, 1);
}

void displayMessage(const string& msg) {
    lock_guard<mutex> lock(displayMutex);
    
    int height = getTerminalHeight();
    int width = getTerminalWidth();
    int maxMessageRow = height - 4;
    
    if (messageRow >= maxMessageRow) {
        messageRow = 4;
        // Clear message area
        for (int i = 4; i < maxMessageRow; i++) {
            moveCursor(i, 1);
            cout << string(width, ' ');
        }
    }
    
    moveCursor(messageRow, 1);
    cout << string(width, ' '); // Clear line
    moveCursor(messageRow, 1);
    
    // Color code messages
    if (msg.find("[ERROR]") != string::npos) {
        setColor("red");
        setColor("bold");
    } else if (msg.find("[SUCCESS]") != string::npos) {
        setColor("green");
        setColor("bold");
    } else if (msg.find("[SYSTEM]") != string::npos) {
        setColor("yellow");
        setColor("bold");
    } else if (msg.find(username + ":") != string::npos) {
        setColor("green");
    } else {
        setColor("cyan");
    }
    
    // Wrap long messages
    if (msg.length() > (size_t)width - 2) {
        cout << msg.substr(0, width - 5) << "...";
    } else {
        cout << msg;
    }
    
    setColor("reset");
    messageRow++;
    
    // Restore cursor to input line
    moveCursor(height - 1, (int)username.length() + 4);
    cout.flush();
}

void receiveMessages() {
    char buffer[4096];
    
    while (running) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        
        if (bytesReceived <= 0) {
            running = false;
            if (authenticated) {
                displayMessage("[SYSTEM] Connection lost!");
            }
            break;
        }
        
        string msg(buffer);
        
        // Check for successful login
        if (msg.find("[SUCCESS] Login successful") != string::npos) {
            authenticated = true;
        }
        
        // Display message during authentication phase
        if (!authenticated) {
            setColor("yellow");
            cout << msg << endl;
            setColor("reset");
            cout.flush();
        } else {
            displayMessage(msg);
        }
    }
}

#ifdef WINDOWS_BUILD
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        running = false;
        closesocket(clientSocket);
        WSACleanup();
        clearScreen();
        setColor("yellow");
        cout << "\n[*] Disconnected from server. Goodbye!\n";
        setColor("reset");
        exit(0);
    }
    return TRUE;
}
#else
void signalHandler(int signal) {
    running = false;
    close(clientSocket);
    clearScreen();
    setColor("yellow");
    cout << "\n[*] Disconnected from server. Goodbye!\n";
    setColor("reset");
    exit(0);
}
#endif

int main() {
#ifdef WINDOWS_BUILD
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, signalHandler);
#endif
    
    clearScreen();
    setColor("cyan");
    setColor("bold");
    cout << "\n+========================================+\n";
    cout << "|   C++ Messenger Client v2.0            |\n";
    cout << "|   With User Authentication             |\n";
#ifdef WINDOWS_BUILD
    cout << "|        Windows Build                   |\n";
#else
    cout << "|        Linux/Unix Build                |\n";
#endif
    cout << "+========================================+\n\n";
    setColor("reset");
    
    setColor("yellow");
    cout << "Connecting to server...\n";
    setColor("reset");
    
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        setColor("red");
        cerr << "Error creating socket!" << endl;
        setColor("reset");
#ifdef WINDOWS_BUILD
        WSACleanup();
#endif
        return 1;
    }
    
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        setColor("red");
        cerr << "Error connecting to server!" << endl;
        setColor("reset");
        close(clientSocket);
#ifdef WINDOWS_BUILD
        WSACleanup();
#endif
        return 1;
    }
    
    setColor("green");
    cout << "Connected to server!\n\n";
    setColor("reset");
    
    // Start receiving thread
    thread receiveThread(receiveMessages);
    receiveThread.detach();
    
    // Wait for welcome message
#ifdef WINDOWS_BUILD
    Sleep(200);
#else
    usleep(200000);
#endif
    
    // Authentication loop
    string input;
    while (!authenticated && running) {
        setColor("cyan");
        cout << "Enter command: ";
        setColor("reset");
        getline(cin, input);
        
        if (!input.empty()) {
            send(clientSocket, input.c_str(), (int)input.length(), 0);
            
            // Wait for server response
#ifdef WINDOWS_BUILD
            Sleep(300);
#else
            usleep(300000);
#endif
            
            // Check if authentication succeeded and extract username
            if (authenticated) {
                size_t firstSpace = input.find(' ');
                size_t secondSpace = input.find(' ', firstSpace + 1);
                if (firstSpace != string::npos && secondSpace != string::npos) {
                    username = input.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                }
                
                // Small delay before switching to UI mode
#ifdef WINDOWS_BUILD
                Sleep(500);
#else
                usleep(500000);
#endif
                break;
            }
        }
    }
    
    if (!authenticated) {
        close(clientSocket);
#ifdef WINDOWS_BUILD
        WSACleanup();
#endif
        return 1;
    }
    
    // Initialize UI after authentication
    drawUI();
    
    // Main input loop
    string message;
    while (running) {
        // Position cursor at input line
        int height = getTerminalHeight();
        moveCursor(height - 1, (int)username.length() + 4);
        
        getline(cin, message);
        
        if (!message.empty()) {
            if (message == "/quit") {
                running = false;
                send(clientSocket, message.c_str(), (int)message.length(), 0);
                break;
            } else if (message == "/clear") {
                messageRow = 4;
                drawUI();
            } else {
                send(clientSocket, message.c_str(), (int)message.length(), 0);
                
                // Clear the input line after sending
                lock_guard<mutex> lock(displayMutex);
                moveCursor(height - 1, 1);
                setColor("green");
                cout << username << " > " << string(getTerminalWidth() - username.length() - 3, ' ');
                setColor("reset");
                moveCursor(height - 1, (int)username.length() + 4);
                cout.flush();
            }
        }
    }
    
    close(clientSocket);
#ifdef WINDOWS_BUILD
    WSACleanup();
#endif
    clearScreen();
    setColor("yellow");
    cout << "\n[*] Disconnected from server. Goodbye!\n";
    setColor("reset");
    
    return 0;
}