// #include "SignalHandler.h"
// #include <iostream>

// SignalHandler::SignalHandler(int port) : port_(port)
// {
//     /*
//     Response of the form:
//     {
//         "new_edge": {
//             "s": double,
//             "d": double,
//             "l": double,
//             "t": double
//         },
//         ?"active_window": {     on first edge and eviction
//             "open": double,
//             "close": double
//         },
//         ?"query_pattern": {     on first edge
//             "pattern": string,
//             "mapping": string
//         },
//         ?"t_edges": [{          batch on eviction, incremental
//             "s": double,
//             "d": double,
//             "l": double,
//             "t": double,
//             "lives": int
//             }, ...],
//         ?"sg_edges": [{         batch on eviction, incremental
//             "s": double,
//             "d": double,
//             "l": double,
//             "t": double,
//             "lives": int
//             }, ...],
//         ?"results": [{          batch
//             "s": double,
//             "d": double,
//             "t": double
//             }, ...],
//         ?"tot_res": int

//     }
//     */
//     CROW_ROUTE(app_, "/proceed")
//         .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST)(
//             [this]()
//             {
//         std::unique_lock<std::mutex> lock(mtx_process_, std::try_to_lock);

//         if (!lock.owns_lock()) {
//             // Mutex is already locked = work in progress
//             return crow::response{429, "Busy"};
//         }
//         cv_process_.notify_one();
//         cv_process_.wait(lock);
//         crow::response resp{200, response_};
//         response_.clear();
//         return resp; });
// }

// void SignalHandler::start()
// {
//     crow_thread_ = std::thread([this]()
//                                {
//         std::cout << "Crow starting on port " << port_ << std::endl;
//         app_.port(port_).multithreaded().run(); });
// }

// void SignalHandler::waitForSignal()
// {
//     cv_process_.notify_one();
//     std::unique_lock<std::mutex> lock(mtx_process_);
//     cv_process_.wait(lock);
// }

// void SignalHandler::stop()
// {
//     cv_process_.notify_all();
//     if (crow_thread_.joinable())
//     {
//         app_.stop();
//         crow_thread_.join();
//         std::cout << "Crow stopped" << std::endl;
//     }
// }

// void SignalHandler::setResponse(const std::string &key, crow::json::wvalue value)
// {
//     response_[key] = std::move(value);
// }

// void SignalHandler::setNestedResponse(const std::string &parentKey, const std::string &childKey, crow::json::wvalue value)
// {
//     response_[parentKey][childKey] = std::move(value);
// }