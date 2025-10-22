#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "crow.h"
#include <atomic>
#include <thread>
#include <chrono>

class SignalHandler
{
public:
    SignalHandler(int port);
    void start();
    void waitForSignal();
    void stop();
    void setResponse(std::string key, std::string value);

private:
    crow::SimpleApp app_;
    int port_;
    std::thread crow_thread_;
    crow::json::wvalue response_;
    std::mutex mtx_process_;
    std::condition_variable cv_process_;
};

#endif // SIGNAL_HANDLER_H