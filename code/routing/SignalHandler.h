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

private:
    crow::SimpleApp app_;
    std::atomic<bool> proceed_flag_;
    int port_;
    std::thread crow_thread_;
};

#endif // SIGNAL_HANDLER_H