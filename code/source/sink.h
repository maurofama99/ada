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
    int matched_paths = 0; // patterns matched

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
        matched_paths++;
    }

    void refresh_resultSet(long long timestamp) {
        // delete all the entries with timestamp less than the given timestamp
        for (auto it = result_set.begin(); it != result_set.end();) {
            auto &destinations = it->second;
            for (auto dest_it = destinations.begin(); dest_it != destinations.end();) {
                if (dest_it->timestamp < timestamp) {
                    dest_it = destinations.erase(dest_it);
                } else {
                    ++dest_it;
                }
            }
            if (destinations.empty()) {
                it = result_set.erase(it);
            } else {
                ++it;
            }
        }
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
        // insert header
        file << "source,destination,timestamp" << std::endl;
        for (const auto &[source, destinations]: result_set) {
            for (const auto &destination: destinations) {
                file << source << "," << destination.destination << "," << destination.timestamp << std::endl;
            }
        }
        file.close();
    }

};

#endif //SINK_H
