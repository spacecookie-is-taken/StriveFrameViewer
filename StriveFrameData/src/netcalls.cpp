#include "netcalls.h"

// Client side C/C++ program to demonstrate Socket
// programming
#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <sstream>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 55455
#define HOST "127.0.0.1"

#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#define OUTPUT_MSG(x) { std::wstringstream msg; msg << x << "\n"; RC::Output::send<LogLevel::Warning>(msg.str()); }

//TODO: make socket non-blocking

struct FrameSupplier {
    bool wsa_failed;
    SOCKET client_fd;
    std::chrono::system_clock::time_point last_connect_try;

    ~FrameSupplier() {
        OUTPUT_MSG("Closing Socket");
        closesocket(client_fd);
    }
    FrameSupplier() : wsa_failed(false), client_fd(INVALID_SOCKET) {
        OUTPUT_MSG("Connecting to: " HOST << ":" << PORT);
        WSADATA wsaData;
        auto startup_status = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (startup_status != 0) {
            OUTPUT_MSG("WSAStartup failed with error: " << startup_status);
            wsa_failed = false;
            return;
        }
        
        Connect();
    }
    bool Reconnect() {
        if (client_fd != INVALID_SOCKET) return true;
        auto time_now = std::chrono::system_clock::now();
        std::chrono::duration<double> time_since_retry = time_now - last_connect_try;
        if (time_since_retry.count() > 10) {
            return Connect();
        }
        return false;
    }
    bool Connect() {
        last_connect_try = std::chrono::system_clock::now();
        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        auto pton_status = inet_pton(AF_INET, HOST, &serv_addr.sin_addr);

        if (pton_status <= 0) {
            OUTPUT_MSG("Invalid Address");
            client_fd = INVALID_SOCKET;
            return false;
        }

        client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == INVALID_SOCKET) {
            OUTPUT_MSG("Socket creation error");
            return false;
        }
        
        auto connect_status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if (connect_status < 0) {
            auto error_code = WSAGetLastError();
            OUTPUT_MSG("Connection Failed: " << error_code);

            closesocket(client_fd);
            client_fd = INVALID_SOCKET;
            return false;
        }
        return true;
    }
    void sendData(const char* data) {
        if (wsa_failed) return;
        if (!Reconnect()) return;

        auto send_status = send(client_fd, data, strlen(data), 0);
        if (send_status < 0) {
            auto error_code = WSAGetLastError();
            OUTPUT_MSG("Send Failed: " << error_code);
            closesocket(client_fd);
            client_fd = INVALID_SOCKET;
        }
    }
};

void SendData(const char* data) {
    static FrameSupplier sender;
    sender.sendData(data);
}

void SendFrameData(int player_id, asw_player* player) {

    std::stringstream msg;
    msg << player_id << player->can_act() << player->is_in_hitstun() << player->is_in_blockstun() << player->is_knockdown()
        << player->is_roll() << player->is_stagger() << player->is_guard_crush() << player->is_active()
        << "_" << player->action_time
        << "|";
    SendData(msg.str().c_str());
}

void SendInputData(const std::vector<bool>& data) {
    std::stringstream msg;
    msg << "i";
    for (const bool b : data) {
        msg << b;
    }
    msg << "|";
    SendData(msg.str().c_str());
}