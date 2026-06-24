#include "ResultAggregator.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cfpe {

double ProcessingStats::throughputMiBPerSecond() const noexcept
{
    if (elapsedSeconds <= 0.0) {
        return 0.0;
    }
    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    return (static_cast<double>(bytesProcessed) / bytesPerMiB) / elapsedSeconds;
}

AggregatedResult ResultAggregator::aggregate(const ThreadPoolResult& result,
                                             std::size_t topN) const
{
    if (topN == 0) {
        throw std::invalid_argument("topN must be greater than zero");
    }

    AggregatedResult aggregated;
    aggregated.stats.bytesProcessed = result.bytesProcessed;
    aggregated.stats.chunksProcessed = result.chunksProcessed;
    aggregated.stats.elapsedSeconds = result.elapsedMs / 1000.0;

    for (const auto& threadResult : result.threadResults) {
        for (const auto& [word, count] : threadResult.frequencies) {
            aggregated.frequencies[word] += count;
            aggregated.totalWords += count;
        }
    }

    aggregated.uniqueWords = aggregated.frequencies.size();
    aggregated.topWords = topWords(aggregated.frequencies, topN);
    return aggregated;
}

std::vector<TopWord> ResultAggregator::topWords(const WordCountMap& frequencies,
                                                std::size_t topN)
{
    std::vector<TopWord> words;
    words.reserve(frequencies.size());
    for (const auto& [word, count] : frequencies) {
        words.push_back({word, count});
    }

    std::sort(words.begin(), words.end(), [](const TopWord& left, const TopWord& right) {
        if (left.count != right.count) {
            return left.count > right.count;
        }
        return left.word < right.word;
    });

    if (words.size() > topN) {
        words.resize(topN);
    }
    return words;
}

} // namespace cfpe
