#include <SFML/Network.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

std::mutex cout_mutex;

const uint8_t kLoginMinLength = 4;
const uint8_t kPasswordMinLength = 4;
const uint8_t kLoginMaxLength = 32;
const uint8_t kPasswordMaxLength = 32;

enum class Responses : uint8_t {
    kSuccessConnection,
    kOk,
    kNoAccess,
    kError,
    kBadLogin,
    kMessage,
    kEmptyResponse,
    kSuccessLogin,
    kLoginIsAlreadyUsed,
    kSuccessSignUp,
    kBadLoginFormat,
};
enum class Queries : uint8_t { kLogin, kEcho, kSignUp, kSignIn, kLogOut, kLoginFormat };

void PrintMessage(std::string msg) {
    cout_mutex.lock();
    std::cout << msg;
    cout_mutex.unlock();
}

bool CheckLoginFormat(std::string& login, std::string& password) {
    if (login.size())
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
    std::string login_;
    std::string password_;
    uint8_t is_admin_;  // Used like bool
};

std::ifstream& operator>>(std::ifstream& in, UserData& data) {
    in >> data.login_;
    in >> data.password_;
    in >> data.is_admin_;
    return in;
}

std::ofstream& operator<<(std::ofstream& out, const UserData& data) {
    out << data.login_ << std::endl;
    out << data.password_ << std::endl;
    out << data.is_admin_;
    return out;
}

class UsersData {
   public:
    void LoadData(std::string DataPath) {
        data_.clear();
        id_by_login_.clear();
        std::ifstream in(DataPath.c_str());
        while (!in.eof()) {
            uint64_t id;
            in >> id;
            UserData user_data;
            in >> user_data;
            data_[id] = user_data;
            id_by_login_[user_data.login_] = id;
        }
    }

    void SaveData(std::string DataPath) {
        std::ofstream out(DataPath.c_str());
        for (auto i : data_) {
            out << i.first << std::endl;
            out << i.second << std::endl;
        }
    }

    std::map<uint64_t, UserData>& Data() {
        return data_;
    }

    uint64_t FindId(std::string login) {
        auto x = id_by_login_.find(login);
        if (x == id_by_login_.end()) {
            return 0;
        }
        return (*x).second;
    }

    Responses SignIn(std::string login, std::string password) {
        auto id = FindId(login);
        if (!id) {
            return Responses::kBadLogin;
        }
        if (data_[id].password_ == password) {
            return Responses::kSuccessLogin;
        }
        return Responses::kBadLogin;
    }

    Responses SignUp(std::string login, std::string password) {
        auto id = FindId(login);
        if (id) {
            return Responses::kLoginIsAlreadyUsed;
        }
        std::random_device gen;
        uint64_t new_id = gen() * gen() - gen();
        while (data_.find(new_id) != data_.end()) {
            new_id = gen() * gen() - gen();
        }
        UserData cur_data;
        cur_data.is_admin_ = false;
        cur_data.password_ = password;
        data_[new_id] = cur_data;
        id_by_login_[login] = new_id;
        return Responses::kSuccessSignUp;
    }

   private:
    std::map<uint64_t, UserData> data_;
    std::map<std::string, uint64_t> id_by_login_;
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
        socket->disconnect();
        delete socket;
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
                sf::Packet rpacket;
                if (!(packet >> msg)) {
                    rpacket << Responses::kError;
                    SendData(socket, rpacket);
                    break;
                }
                rpacket << Responses::kOk << msg;
                SendData(socket, rpacket);
                break;
            }
            case Queries::kLogin: {
                sf::Packet rpacket;
                std::string login;
                std::string password;
                if (!(packet >> login)) {
                    rpacket << Responses::kError;
                    SendData(socket, rpacket);
                    break;
                }
                if (!(packet >> password)) {
                    rpacket << Responses::kError;
                    SendData(socket, rpacket);
                    break;
                }

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
    std::thread cmd_th([&server]() { server.Cmd(); });
    cmd_th.detach();
    server.Listen();
}