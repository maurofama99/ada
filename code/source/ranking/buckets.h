#ifndef ADA_BUCKETS_H
#define ADA_BUCKETS_H

#include <cstdint>
#include <vector>
#include <cstddef>

class RankBuckets {
public:
 using Id = std::int64_t;
 static constexpr double kNoRank = -1;

 RankBuckets(int nVertices, int nEdges);

 void set_rank(Id id, double newRank);
 void remove(Id id);

 [[nodiscard]] std::vector<Id> bottom_k(std::size_t k) const;
 [[nodiscard]] std::vector<Id> top_k(std::size_t k) const;

private:
 struct Meta {
 double rank = kNoRank;
 std::size_t pos =0;
 };

 void remove_from_bucket(Id id, double rank, std::size_t pos);

 int maxRank_;
 std::vector<std::vector<Id>> buckets_;
 std::vector<Meta> meta_;
};

#endif //ADA_BUCKETS_H