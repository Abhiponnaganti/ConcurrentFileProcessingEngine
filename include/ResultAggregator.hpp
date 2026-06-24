#pragma once

#include "Types.hpp"

namespace cfpe {

class ResultAggregator {
public:
    [[nodiscard]] AggregatedResult aggregate(const ThreadPoolResult& result,
                                             std::size_t topN) const;

    [[nodiscard]] static std::vector<TopWord> topWords(const WordCountMap& frequencies,
                                                       std::size_t topN);
};

} // namespace cfpe
