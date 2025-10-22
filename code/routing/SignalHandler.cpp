#include "SignalHandler.h"
#include <iostream>

SignalHandler::SignalHandler(int port) : proceed_flag_(false), port_(port)
{
    CROW_ROUTE(app_, "/proceed")
        .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST)([this]()
                                                                {
        this->proceed_flag_.store(true);
        return "Proceed signal received"; });
}

void SignalHandler::start()
{
    crow_thread_ = std::thread([this]()
                               {
        std::cout << "Crow starting on port " << port_ << std::endl;
        app_.port(port_).multithreaded().run(); });
}

void SignalHandler::waitForSignal()
{
    while (!proceed_flag_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    proceed_flag_.store(false);
}

void SignalHandler::stop()
{
    if (crow_thread_.joinable())
    {
        app_.stop();
        crow_thread_.join();
        std::cout << "Crow stopped" << std::endl;
    }
}