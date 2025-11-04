#include "SignalHandler.h"
#include <iostream>

SignalHandler::SignalHandler(int port) : port_(port)
{
    CROW_ROUTE(app_, "/proceed")
        .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST)(
            [this]()
            {
        std::unique_lock<std::mutex> lock(mtx_process_, std::try_to_lock);
        
        if (!lock.owns_lock()) {
            // Mutex is already locked = work in progress
            return crow::response{429, "Busy"};
        }
        cv_process_.notify_one();
        cv_process_.wait(lock);
        return crow::response{200, response_}; });
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
    cv_process_.notify_one();
    std::unique_lock<std::mutex> lock(mtx_process_);
    cv_process_.wait(lock);
}

void SignalHandler::stop()
{
    cv_process_.notify_all();
    if (crow_thread_.joinable())
    {
        app_.stop();
        crow_thread_.join();
        std::cout << "Crow stopped" << std::endl;
    }
}

void SignalHandler::setResponse(std::string key, std::string value)
{
    response_[key] = value;
}