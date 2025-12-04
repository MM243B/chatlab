#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <thread>        // std::thread
#include <mutex>         // std::mutex, std::lock_guard
#include <fstream>       // std::fstream
#include <algorithm>     // std::remove

#include <winsock2.h>    
#include <ws2tcpip.h>    
#include <windows.h>     
#define close closesocket 

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const int MAX_CONN = 10;
const std::string HISTORY_FILE = "chat_history.txt";

std::vector<int> client_sockets; 
std::mutex clients_mutex;        

void save_message(const std::string& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::ofstream ofs(HISTORY_FILE, std::ios_base::app);
    if (ofs.is_open()) {
        // Добавляем перевод строки, если его нет
        ofs << msg << std::endl; 
    } else {
        std::cerr << " Ошибка: Не удалось открыть файл истории для записи." << std::endl;
    }
}

void send_history(int client_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::ifstream ifs(HISTORY_FILE);
    if (ifs.is_open()) {
        std::string line;
 
        const char* header = "\n--- История переписки ---\n";
        send(client_socket, header, strlen(header), 0);
        
        while (std::getline(ifs, line)) {
            line += "\n";
            if (send(client_socket, line.c_str(), line.length(), 0) == -1) {
              
                break; 
            }
        }
        const char* footer = "---------------------------\n\n";
        send(client_socket, footer, strlen(footer), 0);
    }
}


void broadcast_message(const std::string& msg, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    for (int socket : client_sockets) {
        if (socket != sender_socket) {
        
            std::string msg_to_send = msg + "\n";
            if (send(socket, msg_to_send.c_str(), msg_to_send.length(), 0) == -1) {
                std::cerr << " Ошибка отправки сообщения сокету " << socket << std::endl;
            
            }
        }
    }
}


void handle_client(int client_socket, int client_id) {
    std::cout << " Клиент " << client_id << " подключен (сокет: " << client_socket << ")." << std::endl;
    char buffer[BUFFER_SIZE];
    ssize_t valread;
    std::string user_tag = "[Клиент " + std::to_string(client_id) + "]: ";

    // 1. Добавляем сокет в список активных и отправляем историю
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.push_back(client_socket);
    }
    send_history(client_socket);

    // Цикл приема и рассылки сообщений
    while ((valread = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[valread] = '\0';
        std::string client_msg(buffer);
        
        // Удаляем потенциальные символы новой строки, которые могут прийти из консоли
        if (!client_msg.empty() && client_msg.back() == '\n') client_msg.pop_back();
        if (client_msg.empty()) continue;

        // Форматируем сообщение для истории и рассылки
std::string full_msg = user_tag + client_msg;
        
        // 2. Сохраняем в истории
        save_message(full_msg);
        
        // 3. Рассылаем всем остальным
        broadcast_message(full_msg, client_socket);
        
        std::cout << "Received: " << full_msg << std::endl;

        memset(buffer, 0, BUFFER_SIZE);
    }
    
    if (valread == 0) {
        std::cout << " Клиент " << client_id << " закрыл соединение." << std::endl;
    } else if (valread == -1) {
 
        std::cerr << "Ошибка при приеме данных. Winsock Error: " << WSAGetLastError() << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.erase(
            std::remove(client_sockets.begin(), client_sockets.end(), client_socket), 
            client_sockets.end()
        );
    }

    close(client_socket); 
    std::cout << " Сокет клиента " << client_id << " закрыт." << std::endl;
}

int main() {
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << " WSAStartup failed." << std::endl;
        return EXIT_FAILURE;
    }
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int client_counter = 0; 
    int opt = 1;

    // 1. Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Ошибка: создание сокета не удалось");
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 1.1. Настройка опций сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "Ошибка setsockopt. Winsock Error: " << WSAGetLastError() << std::endl;
        close(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Настройка адреса
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 2. Привязка
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Ошибка: bind не удалось. Winsock Error: " << WSAGetLastError() << std::endl;
        close(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 3. Слушание
    if (listen(server_fd, MAX_CONN) == SOCKET_ERROR) {
        std::cerr << "Ошибка: listen не удалось. Winsock Error: " << WSAGetLastError() << std::endl;
        close(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }
    std::cout << " Чат-сервер запущен и слушает порт " << PORT << "..." << std::endl;

    // 4. Принятие клиентов главный цикл
    while (true) {
       
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) == INVALID_SOCKET) {
            std::cerr << "Ошибка: accept не удалось. Winsock Error: " << WSAGetLastError() << std::endl;
            continue; 
        }
        
        client_counter++;
        
        std::thread client_thread(handle_client, new_socket, client_counter);
        client_thread.detach(); 
    }

    // Очистка 
    WSACleanup();
    close(server_fd);
    return 0;
}