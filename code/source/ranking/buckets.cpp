#include <vector>
#include <cstddef>
#include <algorithm>

#include "buckets.h"

#include <iostream>

// todo: idea per variare nEdges e nVertices a runtime: invece di avere un array di granularità 1, i possibili rank si dividono in range con granularità n^p a seconda delle disponibilità di memoria dell'utente.
// se l'utente disponde di ampia memoria può permettersi un p basso, se invece le risorse sono limitate allora sceglie un p più alto, la conseguenza è che la parte del rank che è più più importante è rappresentata con granularità più fine, mentre la parte meno importante è rappresentata con granularità più grossolana.
// rimane comunque il problema di n, che si può stimare con un periodo di warmup e eventualemente scalare di un fattore proporzionato alla memoria disponibile dall'utente
// esempio: se hai risorse limitate, nel caso degli edge ci sarà una certa soglia in cui grado 100 o grado 1000 saranno entrambi molto impattanti, quindi anche se il p è alto, la granularità è comunque sufficiente a distinguere i rank meno importanti (grado 1-10) da quelli più importanti (grado 100-1000), mentre se hai risorse abbondanti allora puoi permetterti un p più basso e distinguere meglio anche i rank più alti (grado 100 vs grado 101 vs grado 102 etc).
// rimane comunque il fatto che bisogna stimare n, che si può fare con un periodo di warmup in cui si calcola la n media e si scala per un certo numero che è l'expected maximum peak, ovvero mi aspetto che ci potrebbero essere dei picchi fino a 10x
// nel caso in cui ci fossero lo stesso picchi super inaspettati, gli elementi con rank a bassa granularità sono comunque impattanti più dell'expected maximum peak, non facendo differenza


RankBuckets::RankBuckets(int nVertices, int nEdges)
 : maxRank_(std::min(nEdges,2 * (nVertices -1))),
 buckets_(static_cast<std::size_t>(std::min(nEdges,2 * (nVertices -1))) +1),
 meta_(nEdges) {}

void RankBuckets::set_rank(Id id, double newRank) {

    return;

    auto& m = meta_[static_cast<std::size_t>(id)];

    if (m.rank != kNoRank) {
        remove_from_bucket(id, m.rank, m.pos);
    }

    auto& b = buckets_[static_cast<std::size_t>(newRank)];
    m.rank = newRank;
    m.pos = b.size();
    b.push_back(id);
}

void RankBuckets::remove(Id id) {
    return;
    auto& m = meta_[static_cast<std::size_t>(id)];

    if (m.rank == kNoRank) {
        return;
    }

    remove_from_bucket(id, m.rank, m.pos);
    m.rank = kNoRank;
    m.pos =0;
}

std::vector<RankBuckets::Id> RankBuckets::bottom_k(std::size_t k) const {
    return {};
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
    return {};
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
        meta_[static_cast<std::size_t>(moved)].pos = pos;
    }
}
