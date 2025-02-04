#include <SFML/Network.hpp>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::mutex cout_mutex;

enum class Responses : uint8_t {
    kSuccessConnection,
    kOk,
    kNoAccess,
    kError,
    kBadLogin,
    kMessage,
    kEmptyResponse
};
enum class Queries : uint8_t { kLogin, kEcho };

sf::Packet& operator<<(sf::Packet& out, const Responses& rep) {
    return out << static_cast<uint8_t>(rep);
}

sf::Packet& operator>>(sf::Packet& in, Responses& rep) {
    uint8_t ans;
    in >> ans;
    rep = static_cast<Responses>(ans);
    return in;
}

sf::Packet& operator<<(sf::Packet& out, const Queries& rep) {
    return out << static_cast<uint8_t>(rep);
}

sf::Packet& operator>>(sf::Packet& in, Queries& rep) {
    uint8_t ans;
    in >> ans;
    rep = static_cast<Queries>(ans);
    return in;
}

bool SendData(sf::TcpSocket* socket, sf::Packet& packet) {
    if (socket->send(packet) != sf::Socket::Done) {
        return false;
    }
    return true;
}

bool GetData(sf::TcpSocket* socket, sf::Packet& packet) {
    if (socket->receive(packet) != sf::Socket::Done) {
        return false;
    }
    return true;
}

class Data {
   public:
    std::string ip_ = "127.0.0.1";
    int port = 1330;
    std::string login_ = "admin";
    std::string pass_ = "admin";
    sf::TcpSocket* socket_ = nullptr;
};

void PrintMessage(std::string msg) {
    cout_mutex.lock();
    std::cout << msg;
    cout_mutex.unlock();
}

void CommandHandler() {
    Data data;
    PrintMessage("Client is ready. Type command \"help\" to get command list");
    for(;;) {
        std::string com;
        getline(std::cin, com);
        if (com == "help") {
            PrintMessage("Command list:\nhelp - get command list\nlogin - configure login\npassword - configure password\nip - configure server ip\nport - configure server port");
            continue;
        }
        if (com == "login") {
            PrintMessage("Current login is " + data.login_ + ".\nEnter new login (or just press ENTER, if won't change login)");
            std::string new_login;
            getline(std::cin, new_login);
            if (new_login.size() != 0) {
                data.login_ = new_login;
            }
            continue;
        }
        if (com == "password") {
            PrintMessage("Current password is " + data.pass_ + ".\nEnter new password (or just press ENTER, if won't change password)");
            std::string new_pass;
            getline(std::cin, new_pass);
            if (new_pass.size() != 0) {
                data.pass_ = new_pass;
            }
            continue;
        }
    }
}

int main() {
    std::cout << "Starting command handler" << std::endl;
    std::thread command_handler_th(CommandHandler);
    command_handler_th.join();
}