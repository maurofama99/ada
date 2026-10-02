#ifndef SINK_H
#define SINK_H
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class Sink {
    // Keep every timestamp reported for each source-destination pair.
    std::unordered_map<long long, std::unordered_map<long long, std::vector<long long>>> result_set;

public:
    int matched_paths = 0; // patterns matched
    unsigned long total_matches = 0;

    // get result set size
    long long getResultSetSize() {
        long long size = 0;
        for (const auto &[source, destinations]: result_set) {
            // count distinct active pairs.
            size += destinations.size();
        }
        return size;
    }

    // add entry in result set
    void addEntry(long long source, long long destination, long long timestamp) {
        auto& destinations = result_set[source];
        auto [destination_it, inserted] = destinations.try_emplace(destination);
        if (inserted) {
            // Count a pair when it first becomes active.
            matched_paths++;
        }

        // Store every match occurrence, including duplicate timestamps.
        destination_it->second.push_back(timestamp);
        total_matches++;
    }

    void printResultSet() {
        for (const auto &[source, destinations]: result_set) {
            for (const auto &[destination, timestamps]: destinations) {
                for (const long long timestamp: timestamps) {
                    std::cout << "Path from " << source << " to " << destination
                              << " at time " << timestamp << std::endl;
                }
            }
        }
    }

    // export the result set into a file in form of csv with columns source, destination, timestamp
    void exportResultSet(const std::string &filename) {
        std::ofstream file(filename);
        // insert header
        for (const auto &[source, destinations]: result_set) {
            for (const auto &[destination, timestamps]: destinations) {
                for (const long long timestamp: timestamps) {
                    file << source << " " << destination << " " << timestamp << std::endl;
                }
            }
        }
        file.close();
    }

};

#endif //SINK_H
