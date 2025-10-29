#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "crow.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <crow/middlewares/cors.h>

class SignalHandler
{
public:
    SignalHandler(int port);
    void start();
    void waitForSignal();
    void stop();
    void setResponse(std::string key, std::string value);

    template <typename T>
    void setNestedResponse(const std::string &parentKey, const std::string &childKey, T value)
    {
        response_[parentKey][childKey] = std::forward<T>(value);
    }

private:
    crow::App<crow::CORSHandler> app_;
    int port_;
    std::thread crow_thread_;
    crow::json::wvalue response_;
    std::mutex mtx_process_;
    std::condition_variable cv_process_;
};

#endif // SIGNAL_HANDLER_H