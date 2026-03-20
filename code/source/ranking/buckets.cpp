#include <vector>
#include <cstddef>
#include <iostream>

#include "buckets.h"

void RankBuckets::set_rank(Id id, int newRank) {

    auto&[rank, pos] = meta_[id];

    if (rank != kNoRank) {
        remove_from_bucket(id, rank, pos);
    }

    auto& b = buckets_[static_cast<std::size_t>(newRank)];
    rank = newRank;
    pos = b.size();
    b.push_back(id);
}

void RankBuckets::remove(Id id) {
    auto&[rank, pos] = meta_[id];

    if (rank == kNoRank) {
        return;
    }

    remove_from_bucket(id, rank, pos);
    rank = kNoRank;
    pos =0;
}

std::vector<RankBuckets::Id> RankBuckets::bottom_k(std::size_t k) const {
    std::vector<Id> out;
    out.reserve(k);

    for (std::size_t r =0; r < buckets_.size() && out.size() < k; ++r) {
        const auto& b = buckets_[r];
        for (Id id : b) {
            out.push_back(id);
            if (out.size() == k) {
                break;
            }
        }
    }

    return out;
}

std::vector<RankBuckets::Id> RankBuckets::top_k(std::size_t k) const {
    std::vector<Id> out;
    out.reserve(k);

    for (std::size_t r = buckets_.size(); r >0 && out.size() < k; --r) {
        const auto& b = buckets_[r -1];
        for (Id id : b) {
            out.push_back(id);
            if (out.size() == k) {
                break;
            }
        }
    }

    return out;
}

void RankBuckets::remove_from_bucket(Id id, double rank, std::size_t pos) {
    auto& b = buckets_[static_cast<std::size_t>(rank)];

    const Id moved = b.back();
    b[pos] = moved;
    b.pop_back();

    if (moved != id) {
        meta_[moved].pos = pos;
    }
}
