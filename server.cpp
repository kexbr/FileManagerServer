#include <SFML/Network.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <variant>

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
enum class Queries : uint8_t { kLogin, kEcho, kSignUp };

void PrintMessage(std::string msg) {
    cout_mutex.lock();
    std::cout << msg;
    cout_mutex.unlock();
}

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

class UserData {
   public:
    friend std::ifstream& operator>>(std::ifstream& in, UserData& data);
    friend std::ofstream& operator<<(std::ofstream& in, const UserData& data);

   private:
    std::string password_;
    uint8_t is_admin_; // Used like bool
};

std::ifstream& operator>>(std::ifstream& in, UserData& data) {
    in >> data.password_;
    in >> data.is_admin_;
    return in;
}

std::ofstream& operator<<(std::ofstream& out, const UserData& data) {
    out << data.password_;
    out << data.is_admin_;
    return out;
}

class UsersData {
   public:
    void LoadData(std::string DataPath) {
        std::ifstream in(DataPath.c_str());
        while (!in.eof()) {
            std::string user_name;
            in >> user_name;
            UserData user_data;
            in >> user_data;
            data_[user_name] = user_data;
        }
    }

    void SaveData(std::string DataPath) {
        std::ofstream out(DataPath.c_str());
        for (auto i : data_) {
            out << i.first << std::endl;
            out << i.second << std::endl;
        }
    }

    std::map<std::string, UserData>& Data() {
        return data_;
    }

   private:
    std::map<std::string, UserData> data_;
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
            PrintMessage("User connected\n");
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
        PrintMessage("Packet size: " + std::to_string(packet.getDataSize()) + '\n');
        Queries que;
        if (!(packet >> que)) {
            PrintMessage("Bad packet!" + std::to_string(packet.getDataSize()) + '\n');
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
        PrintMessage("Query was answered.\n");
    }

    void ClientHandler(sf::TcpSocket* socket) {
        for (;;) {
            sf::Packet r_packet;
            if (GetData(socket, r_packet) == true) {
                PrintMessage("Got data\n");
                HandleQuery(socket, r_packet);
            } else {
                DeleteUser(socket);
                break;
            }
            r_packet.clear();
        }
    }

    std::mutex user_vector_mutex_;
    std::vector<sf::TcpSocket*>
        cur_users_;  // TODO: optimize current users structure by using oset to reach O(logN)
                     // complexity in adding and deleting users.
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