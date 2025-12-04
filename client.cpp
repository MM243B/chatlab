#include <iostream>
#include <string>
#include <cstring>
#include <thread>        // std::thread

#include <winsock2.h>    
#include <ws2tcpip.h>    
#include <windows.h>     
#define close closesocket 

const int PORT = 8080;
const int BUFFER_SIZE = 1024;
const char* SERVER_IP = "127.0.0.1";

volatile bool running = true; 

/**
 * @brief Поток для приема входящих сообщений от сервера.
 */
void receive_messages(int sock) {
    char buffer[BUFFER_SIZE];
    int valread;
    
    while (running) {
        valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (valread > 0) {
            buffer[valread] = '\0';
            std::cout << "\n" << buffer;
            std::cout << ">> "; 
            std::cout.flush(); 
        } else if (valread == 0) {
            std::cout << "\n Сервер закрыл соединение. Завершение работы." << std::endl;
            running = false;
            break;
        } else if (valread == -1) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                std::cerr << "Ошибка при получении данных. Winsock Error: " << WSAGetLastError() << std::endl;
                running = false;
                break;
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << " WSAStartup failed." << std::endl;
        return EXIT_FAILURE;
    }
    
    int sock = 0;
    struct sockaddr_in serv_addr;
    std::string user_input;

    // 1. Создание сокета
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Ошибка: создание сокета не удалось. Winsock Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Ошибка: неверный IP-адрес." << std::endl;
        close(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // 2. Соединение с сервером
    std::cout << " Попытка соединения с чат-сервером..." << std::endl;
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        std::cerr << "Ошибка: соединение не удалось. Winsock Error: " << WSAGetLastError() << ". Убедитесь, что сервер запущен." << std::endl;
        close(sock);
        WSACleanup();
        return EXIT_FAILURE;
    }
    std::cout << " Соединение установлено. Начните чат (введите 'exit' для выхода)." << std::endl;
    
    std::thread receiver_thread(receive_messages, sock);
    
    while (running) {
        std::cout << ">> ";
        std::getline(std::cin, user_input);
        
        if (!running) break; 
        
        if (user_input == "exit") {
            running = false;
            break;
        }

        if (user_input.empty()) continue;

        // 3. Отправка сообщения
        if (send(sock, user_input.c_str(), user_input.length(), 0) == -1) {
            std::cerr << "Ошибка при отправке сообщения. Winsock Error: " << WSAGetLastError() << std::endl;
            running = false;
        }
    }

    // Ожидание завершения потока приема и очистка
if (receiver_thread.joinable()) {
        // Закрытие сокета принудительно разблокирует recv() в другом потоке
        shutdown(sock, SD_BOTH); 
        receiver_thread.join();
    }
    
    // 4. Закрытие дескриптора
    close(sock);
    WSACleanup();
    std::cout << "Клиент завершил работу." << std::endl;

    return 0;
}