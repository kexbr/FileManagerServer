/*TODO:
1) fix load method
2) replace ptrs with shared_ptrs
3) write file managment
4) make administrator methods
*/

#include <SFML/Network.hpp>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <mutex>
#include <random>
#include <thread>
#include "constants.h"
#include "enums.h"
#include "packet_overloads.h"

std::mutex cout_mutex;
std::atomic<int> reader_count_{0};

void PrintMessage(std::string msg) {
    cout_mutex.lock();
    std::cout << msg;
    cout_mutex.unlock();
}

bool CheckLoginFormat(std::string& login, std::string& password) {
    if (login.size() < kLoginMinLength || login.size() > kLoginMaxLength ||
        password.size() < kPasswordMinLength || password.size() > kPasswordMaxLength) {
        return false;
    }
    for (const auto& ch : login) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
            return false;
        }
    }
    for (const auto& ch : password) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
            return false;
        }
    }
    return true;
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

std::ostream& operator<<(std::ostream& out, const UserData& data) {
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
            return Responses::kSuccessSignIn;
        }
        return Responses::kBadLogin;
    }

    Responses SignUp(std::string login, std::string password, uint64_t& ans_id) {
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
        cur_data.login_ = login;
        data_[new_id] = cur_data;
        id_by_login_[login] = new_id;
        ans_id = new_id;
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

    void Cmd();

    void Listen();

    std::mutex& GetUserVectorMutex() {
        return user_vector_mutex_;
    }

    std::vector<sf::TcpSocket*>& GetCurUsers() {
        return cur_users_;
    }

    class FileClass;

   private:
    void DeleteUser(sf::TcpSocket* socket);

    bool SendData(sf::TcpSocket* socket, sf::Packet& packet);

    bool GetData(sf::TcpSocket* socket, sf::Packet& packet);

    void HandleQuery(sf::TcpSocket* socket, sf::Packet& packet);

    void ClientHandler(sf::TcpSocket* socket);

    FileStatus SendFile(
        sf::TcpSocket* socket, std::string filename, uint64_t start_pos);

    std::mutex user_vector_mutex_;
    std::vector<sf::TcpSocket*>
        cur_users_;  // TODO: optimize current users structure by using oset to reach O(logN)
                     // complexity in adding and deleting users.
    std::vector<uint64_t> user_id_;
    uint16_t port_;
    UsersData users_data_;
};

void Server::Cmd() {
    PrintMessage("Started servers cmd. Type help for list of commands.");
    std::string s;
    {
        std::string dummy;
        getline(std::cin, dummy);
    }
    for (;;) {
        std::string s;
        std::cin >> s;
        if (s == "save") {
            std::string filepath;
            std::cin >> filepath;
            PrintMessage("Saving users data.");
            users_data_.SaveData(filepath);
        }
        if (s == "load") {
            std::string filepath;
            std::cin >> filepath;
            PrintMessage("Loading users data.");
            user_vector_mutex_.lock();
            for (auto& i : cur_users_) {
                i->disconnect();
                delete i;
            }
            cur_users_.clear();
            user_id_.clear();
            user_vector_mutex_.unlock();
            users_data_.LoadData(filepath);
        }
        if (s == "check") {
            for (auto& i : users_data_.Data()) {
                std::cout << i.first << std::endl;
                std::cout << i.second << std::endl;
            }
        }
    }
}

void Server::Listen() {
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
        user_id_.push_back(0);
        user_vector_mutex_.unlock();
        std::thread client_thread([this, client]() { this->ClientHandler(client); });
        client_thread.detach();
    }
}

void Server::DeleteUser(sf::TcpSocket* socket) {
    if (socket != nullptr) {
        user_vector_mutex_.lock();
        auto position = std::find(cur_users_.begin(), cur_users_.end(), socket);
        if (position != cur_users_.end()) {
            user_id_.erase(user_id_.begin() + (position - cur_users_.begin()));
            cur_users_.erase(position);
        }
        user_vector_mutex_.unlock();
        socket->disconnect();
        delete socket;
        socket = nullptr;
    }
}

bool Server::SendData(sf::TcpSocket* socket, sf::Packet& packet) {
    if (socket->send(packet) != sf::Socket::Done) {
        return false;
    }
    packet.clear();
    return true;
}

bool Server::GetData(sf::TcpSocket* socket, sf::Packet& packet) {
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

void Server::HandleQuery(sf::TcpSocket* socket, sf::Packet& packet) {
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
        case Queries::kSignIn: {
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
            if (!CheckLoginFormat(login, password)) {
                rpacket << Responses::kBadLoginFormat;
                SendData(socket, rpacket);
                break;
            }
            auto res = users_data_.SignIn(login, password);
            if (res == Responses::kSuccessSignIn) {
                user_vector_mutex_.lock();
                size_t position =
                    std::find(cur_users_.begin(), cur_users_.end(), socket) - cur_users_.begin();
                user_id_[position] = users_data_.FindId(login);
                user_vector_mutex_.unlock();
            }
            rpacket << res;
            SendData(socket, rpacket);
            break;
        }
        case Queries::kSignUp: {
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
            if (!CheckLoginFormat(login, password)) {
                rpacket << Responses::kError;
                SendData(socket, rpacket);
                break;
            }
            uint64_t new_id = 0;
            auto res = users_data_.SignUp(login, password, new_id);
            if (res != Responses::kSuccessSignUp) {
                rpacket << res;
                SendData(socket, rpacket);
                break;
            }
            user_vector_mutex_.lock();
            size_t position =
                std::find(cur_users_.begin(), cur_users_.end(), socket) - cur_users_.begin();
            user_id_[position] = users_data_.FindId(login);
            user_vector_mutex_.unlock();
            rpacket << res;
            SendData(socket, rpacket);
            break;
        }
        case Queries::kLogOut: {
            user_vector_mutex_.lock();
            size_t position =
                std::find(cur_users_.begin(), cur_users_.end(), socket) - cur_users_.begin();
            user_id_[position] = 0;
            user_vector_mutex_.unlock();
            break;
        }
        case Queries::kLoginFormat: {
            sf::Packet rpacket;
            rpacket << Responses::kOk;
            rpacket << kLoginMinLength << kLoginMaxLength << kPasswordMinLength
                    << kPasswordMaxLength;
            SendData(socket, rpacket);
            break;
        }
        case Queries::kSendFile: {
            std::string filename;
            uint64_t recieved;
            sf::Packet rpacket;
            if (!(packet >> filename)) {
                DeleteUser(socket);
                return;
            }
            if (!(packet >> recieved)) {
                DeleteUser(socket);
                return;
            }
            SendFile(socket, filename, recieved);
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

void Server::ClientHandler(sf::TcpSocket* socket) {
    for (;;) {
        sf::Packet r_packet;
        if (GetData(socket, r_packet) == true) {
            PrintMessage("Got data\n");
            HandleQuery(socket, r_packet);
        } else {
            return;
        }
        r_packet.clear();
    }
}

FileStatus Server::SendFile(
    sf::TcpSocket* socket, std::string filename, uint64_t start_pos) {
    if (reader_count_.load() >= kMaxOpenedFiles) {
        return FileStatus::kBusy;
    }
    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return FileStatus::kNotFound;
    }
    reader_count_++;
    sf::Packet packet;
    std::streampos pos = in.tellg();
    uint64_t filesize = static_cast<uint64_t>(pos);
    packet << Responses::kFileSize;
    packet << filesize;
    if (!SendData(socket, packet)) {
        reader_count_--;
        return FileStatus::kConnectionError;
    }
    in.seekg(start_pos, std::ios::beg);
    packet.clear();
    while (!in.eof()) {
        std::string buf;
        while ((!in.eof()) && buf.size() < kFileBlockSize) {
            char c = in.get();
            buf.push_back(c);
        }
        packet << Responses::kFileData << buf;
        if (!SendData(socket, packet)) {
            in.close();
            reader_count_--;
            return FileStatus::kConnectionError;
        }
        packet.clear();
    }
    in.close();
    reader_count_--;
    return FileStatus::kDone;
}

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