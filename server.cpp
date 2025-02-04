#include <SFML/Network.hpp>
#include <cstdint>
#include <iostream>
#include <mutex>
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

class ClientInteraction {
   public:
   private:
};

class Server {
   public:
    Server(uint16_t port) : port_(port) {
    }

    void Listen() {
        cout_mutex.lock();
        std::cout << "Opening port" << std::endl;
        cout_mutex.unlock();
        sf::TcpListener listener;
        if (listener.listen(port_) != sf::Socket::Done) {
            throw std::runtime_error(
                std::string("An error has occured during listening port") + std::to_string(port_));
        }
        for (;;) {
            sf::TcpSocket* client = new sf::TcpSocket;
            if (listener.accept(*client) != sf::Socket::Done) {
                throw std::runtime_error(std::string(
                    "An error has occured during accepting the "
                    "clients connection"));
            }
            cout_mutex.lock();
            std::cout << "User connected" << std::endl;
            cout_mutex.unlock();
            user_vector_mutex_.lock();
            cur_users_.push_back(client);
            user_id_.push_back(-1);
            user_vector_mutex_.unlock();
            std::thread client_thread([this, client]() { this->ClientHandler(client); });
            client_thread.detach();
        }
    }

    std::mutex& GetUserVectorMutex() {
        return user_vector_mutex_;
    }

    std::vector<sf::TcpSocket*>& GetCurUsers() {
        return cur_users_;
    }

   private:
    void DeleteUser(sf::TcpSocket* socket) {
        user_vector_mutex_.lock();
        size_t position =
            std::find(cur_users_.begin(), cur_users_.end(), socket) - cur_users_.begin();
        cur_users_.erase(cur_users_.begin() + position);
        user_id_.erase(user_id_.begin() + position);
        user_vector_mutex_.unlock();
    }

    bool SendData(sf::TcpSocket* socket, sf::Packet& packet) {
        if (socket->send(packet) != sf::Socket::Done) {
            return false;
        }
        packet.clear();
        return true;
    }

    bool GetData(sf::TcpSocket* socket, sf::Packet& packet) {
        if (socket->receive(packet) != sf::Socket::Done) {
            DeleteUser(socket);
            packet.clear();
            return false;
        }
        if (packet.getDataSize() > 1024) {
            DeleteUser(socket);
            packet.clear();
            return false;
        }
        return true;
    }

    void HandleQuery(sf::TcpSocket* socket, sf::Packet& packet) {
        cout_mutex.lock();
        std::cout << "Packet size: " << packet.getDataSize() << std::endl;
        cout_mutex.unlock();
        Queries que;
        if (!(packet >> que)) {
            cout_mutex.lock();
            std::cout << "Bad packet!" << packet.getDataSize() << std::endl;
            cout_mutex.unlock();
            packet << Responses::kError;
            SendData(socket, packet);
            return;
        }
        switch (que) {
            case Queries::kEcho: {
                std::string msg;
                if (!(packet >> msg)) {
                    packet << Responses::kError;
                    SendData(socket, packet);
                    return;
                }
                packet.clear();
                packet << Responses::kOk << msg;
                SendData(socket, packet);
                break;
            }
            default: {
                packet.clear();
                packet << Responses::kEmptyResponse;
                SendData(socket, packet);
                break;
            }
        }
        cout_mutex.lock();
        std::cout << "Query was answered." << std::endl;
        cout_mutex.unlock();
    }

    void ClientHandler(sf::TcpSocket* socket) {
        for (;;) {
            sf::Packet r_packet;
            if (GetData(socket, r_packet) == true) {
                std::cout << "Got data" << std::endl;
                HandleQuery(socket, r_packet);
            } else {
                DeleteUser(socket);
                break;
            }
            r_packet.clear();
        }
    }

    std::mutex user_vector_mutex_;
    std::vector<sf::TcpSocket*> cur_users_;
    std::vector<uint64_t> user_id_;
    uint16_t port_;
};

int main() {
    std::cout << "Starting server" << std::endl;
    std::cout << "Enter port:" << std::endl;
    uint16_t port = 0;
    std::cin >> port;
    Server server(port);
    server.Listen();
}