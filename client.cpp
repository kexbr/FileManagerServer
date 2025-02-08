#include <SFML/Network.hpp>
#include <chrono>
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
    kEmptyResponse,
    kSuccessSignIn,
    kLoginIsAlreadyUsed,
    kSuccessSignUp,
    kBadLoginFormat,
};
enum class Queries : uint8_t { kEcho, kSignUp, kSignIn, kLogOut, kLoginFormat };

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
    uint16_t port_ = 1330;
    std::string login_ = "admin";
    std::string pass_ = "admin";
    sf::TcpSocket* socket_ = nullptr;
};

void PrintMessage(std::string msg) {
    cout_mutex.lock();
    std::cout << msg << std::endl;
    cout_mutex.unlock();
}

void TerminateSocket(Data& data) {
    data.socket_->disconnect();
    delete data.socket_;
    data.socket_ = nullptr;
}

void HelpFunc(Data& data) {
    PrintMessage(
        "Command list:\nhelp - get command list\nlogin - configure login\npassword - "
        "configure password\nip - configure server ip\nport - configure server port\necho "
        "- echo connected server\n");
    PrintMessage(
        "connect - connect to the server with ip " + data.ip_ + " and port " +
        std::to_string(data.port_) + "\n");
    return;
}

void LoginChangeFunc(Data& data) {
    PrintMessage(
        "Current login is " + data.login_ +
        ".\nEnter new login (or just press ENTER, if won't change login)\n");
    std::string new_login;
    getline(std::cin, new_login);
    if (new_login.size() != 0) {
        data.login_ = new_login;
    }
    return;
}

void PasswordChangeFunc(Data& data) {
    PrintMessage(
        "Current password is " + data.pass_ +
        ".\nEnter new password (or just press ENTER, if won't change password)\n");
    std::string new_pass;
    getline(std::cin, new_pass);
    if (new_pass.size() != 0) {
        data.pass_ = new_pass;
    }
    return;
}

void IpChangeFunc(Data& data) {
    PrintMessage(
        "Current ip is " + data.ip_ +
        ".\nEnter new ip (or just press ENTER, if won't change ip)\n");
    std::string new_ip;
    getline(std::cin, new_ip);
    if (new_ip.size() != 0) {
        data.ip_ = new_ip;
    }
    return;
}

void PortChangeFunc(Data& data) {
    PrintMessage(
        "Current port is " + std::to_string(data.port_) +
        ".\nEnter new port (or just enter 0, if won't change port)\n");
    uint16_t new_port;
    std::cin >> new_port;
    if (new_port != 0) {
        data.port_ = new_port;
    }
    return;
}

bool ConnectToServer(Data& data, bool output_flag) {
    if (output_flag) {
        PrintMessage("Connecting to the server...");
    }
    data.socket_ = new sf::TcpSocket;
    if (data.socket_->connect(data.ip_, data.port_) != sf::Socket::Done) {
        if (output_flag) {
            PrintMessage("Something went wrong during connection to the server!");
        }
        TerminateSocket(data);
    } else {
        if (output_flag) {
            PrintMessage("Connected successfully!");
        }
        return true;
    }
    return false;
}

bool EmptySocket(Data& data, bool output_flag) {
    if (data.socket_ == nullptr) {
        if (output_flag) {
            PrintMessage("There is no stable connection to the server! Need to connect first.\n");
        }
        return true;
    }
    return false;
}

bool EchoFunc(Data& data, bool output_flag) {
    if (data.socket_ == nullptr) {
        if (output_flag) {
            PrintMessage(
                "There is no stable connection to the server! Use connect to connect to the "
                "server\n");
        }
    } else {
        sf::Packet packet;
        packet << Queries::kEcho;
        while (packet.getDataSize() < 64) {
            packet << '0';
        }
        auto start_time = std::chrono::high_resolution_clock::now();
        if (data.socket_->send(packet) != sf::Socket::Done) {
            if (output_flag) {
                PrintMessage("Something went wrong while sending the package. Disconnecting.");
            }
            TerminateSocket(data);
        }
        if (data.socket_->receive(packet) != sf::Socket::Done) {
            if (output_flag) {
                PrintMessage("Something went wrong while recieving the package. Disconnecting.");
            }
            TerminateSocket(data);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto result = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        if (output_flag) {
            PrintMessage(
                "Sent and recieved package with size " + std::to_string(packet.getDataSize()) +
                " in " + std::to_string(result.count()) + " microseconds");
        }
        return true;
    }
    return false;
}

void LogFormatFunc(Data& data, bool output_flag) {
    if (EmptySocket(data, output_flag)) {
        return;
    }
    sf::Packet packet;
    packet << Queries::kLoginFormat;
    SendData(data.socket_, packet);
    packet.clear();
    GetData(data.socket_, packet);
    Responses rep;
    if (!(packet >> rep)) {
        PrintMessage("Bad packet recieved!");
        return;
    }
    if (rep != Responses::kOk) {
        PrintMessage("Something went wrong.");
        return;
    }
    uint8_t lmin, lmax, pmin, pmax;
    if (!(packet >> lmin >> lmax >> pmin >> pmax)) {
        PrintMessage("Bad packet recieved!");
        return;
    }
    PrintMessage("Login minimum length: " + std::to_string(lmin));
    PrintMessage("Login maximum length: " + std::to_string(lmax));
    PrintMessage("Password minimum length: " + std::to_string(pmin));
    PrintMessage("Password maximum length: " + std::to_string(pmax));
    return;
}

void SignInFunc(Data& data, bool output_flag) {
    if (EmptySocket(data, output_flag)) {
        return;
    }
    sf::Packet packet;
    packet << Queries::kSignIn;
    packet << data.login_ << data.pass_;
    SendData(data.socket_, packet);
    sf::Packet rpacket;
    Responses rep;
    if (!GetData(data.socket_, rpacket)) {
        PrintMessage("Something went wrong during recieving package");
        return;
    }
    if (!(rpacket >> rep)) {
        PrintMessage("Recieved bad package!");
        return;
    }
    if (rep == Responses::kBadLoginFormat) {
        PrintMessage("Bad login or password format!");
        return;
    }
    if (rep == Responses::kBadLogin) {
        PrintMessage("Login or password is/are incorrect!");
        return;
    }
    if (rep == Responses::kSuccessSignIn) {
        PrintMessage("Signed in successfully!");
        return;
    }
}

void SignUpFunc(Data& data, bool output_flag) {
    if (EmptySocket(data, output_flag)) {
        return;
    }
    sf::Packet packet;
    packet << Queries::kSignUp;
    packet << data.login_;
    packet << data.pass_;
    SendData(data.socket_, packet);
    sf::Packet rpacket;
    if (!GetData(data.socket_, rpacket)) {
        PrintMessage("Something went wrong during recieving package.");
        return;
    }
    Responses rep;
    if (!(rpacket >> rep)) {
        PrintMessage("Bad packet.");
        return;
    }
    if (rep == Responses::kError) {
        PrintMessage("Error from the server has occured");
        return;
    }
    if (rep == Responses::kEmptyResponse) {
        PrintMessage("Server hasn't sent anything");
        return;
    }
    if (rep == Responses::kBadLoginFormat) {
        PrintMessage("Bad login or password format!");
        return;
    }
    if (rep == Responses::kLoginIsAlreadyUsed) {
        PrintMessage("Login is already used!");
        return;
    }
    if (rep == Responses::kSuccessSignUp) {
        PrintMessage("Signed up successfully!");
        return;
    }
}

void CommandHandler() {
    Data data;
    PrintMessage("Client is ready. Type command \"help\" to get command list");
    for (;;) {
        std::string com;
        getline(std::cin, com);
        if (com == "help") {
            HelpFunc(data);
        }
        if (com == "login") {
            LoginChangeFunc(data);
        }
        if (com == "password") {
            PasswordChangeFunc(data);
        }
        if (com == "ip") {
            IpChangeFunc(data);
        }
        if (com == "port") {
            PortChangeFunc(data);
        }
        if (com == "connect") {
            ConnectToServer(data, true);
        }
        if (com == "echo") {
            EchoFunc(data, true);
        }
        if (com == "login_format") {
            LogFormatFunc(data, true);
        }
        if (com == "signin") {
            SignInFunc(data, true);
        }
        if (com == "signup") {
            SignUpFunc(data, true);
        }
    }
}

int main() {
    std::cout << "Starting command handler" << std::endl;
    std::thread command_handler_th(CommandHandler);
    command_handler_th.join();
}