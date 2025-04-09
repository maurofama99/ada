#ifndef SINK_H
#define SINK_H
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>

struct result {
    long long destination;
    long long timestamp;

    bool operator==(const result &other) const {
        return destination == other.destination;
    }
};

struct resultHash {
    size_t operator()(const result &p) const {
        return std::hash<long long>()(p.destination);
    }
};

class Sink {
    std::unordered_map<long long, std::unordered_set<result, resultHash> > result_set;
public:

    // get result set size
    long long getResultSetSize() {
        long long size = 0;
        for (const auto &[source, destinations]: result_set) {
            size += destinations.size();
        }
        return size;
    }

    // add entry in result set
    void addEntry(long long source, long long destination, long long timestamp) {
        result res = {destination, timestamp};
        result_set[source].insert(res);
    }

    void printResultSet() {
        for (const auto &[source, destinations]: result_set) {
            for (const auto &destination: destinations) {
                std::cout << "Path from " << source << " to " << destination.destination << " at time " << destination.
                        timestamp << std::endl;
            }
        }
    }

    // export the result set into a file in form of csv with columns source, destination, timestamp
    void exportResultSet(const std::string &filename) {
        std::ofstream file(filename);
        for (const auto &[source, destinations]: result_set) {
            for (const auto &destination: destinations) {
                file << source << "," << destination.destination << "," << destination.timestamp << std::endl;
            }
        }
        file.close();
    }

};

#endif //SINK_H
