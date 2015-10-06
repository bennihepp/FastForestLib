#pragma once

#include <vector>
#include <cmath>
#include <numeric>

#include "ait.h"

namespace ait
{

/// @brief A histogram over the classes of samples.
template <typename TSample>
class HistogramStatistics
{
    std::vector<size_type> histogram_;
    size_type num_of_samples_;

public:

    class HistogramStatisticsFactory
    {
        size_type num_of_classes_;

    public:
        using value_type = HistogramStatistics;

        HistogramStatisticsFactory(size_type num_of_classes)
        : num_of_classes_(num_of_classes)
        {}

        HistogramStatistics create() const
        {
            return HistogramStatistics(num_of_classes_);
        }
    };
    
    /// @brief Create an empty histogram.
    HistogramStatistics()
    : histogram_(0, 0), num_of_samples_(0)
    {}

    /// @brief Create an empty histogram.
    /// @param num_of_classes The number of classes.
    HistogramStatistics(size_type num_of_classes)
    : histogram_(num_of_classes, 0), num_of_samples_(0)
    {}

    /// @brief Create a histogram from a vector of counts per class.
    HistogramStatistics(const std::vector<size_type> &histogram)
    : histogram_(histogram),
    num_of_samples_(std::accumulate(histogram.cbegin(), histogram.cend(), 0))
    {}

    void lazy_accumulate(const TSample &sample)
    {
        size_type label = sample.get_label();
        histogram_[label]++;
    }

    void finish_lazy_accumulation()
    {
        compute_num_of_samples();
    }

    void accumulate(const TSample &sample)
    {
        size_type label = sample.get_label();
        histogram_[label]++;
        num_of_samples_++;
    }

    template <typename T>
    void accumulate(T it_start, T it_end)
    {
        for (T it = it_start; it != it_end; ++it)
        {
            lazy_accumulate(*it);
        }
        num_of_samples_ += it_end - it_start;
    }

    template <typename T>
    void accumulate_histograms(T it_start, T it_end)
    {
        for (T it = it_start; it != it_end; ++it)
        {
            for (size_type i=0; i < it->histogram_.size(); i++)
            {
                histogram_[i] += it->histogram_[i];
            }
        }
        finish_lazy_accumulation();
    }

    /// @brief Return the numbers of samples contributing to the histogram.
    size_type num_of_samples() const
    {
        return num_of_samples_;
    }

    /// @brief Return the vector of counts per class.
    const std::vector<size_type> & get_histogram() const
    {
        return histogram_;
    }

    /// @return: The Shannon entropy of the histogram.
    const scalar_type entropy() const
    {
        scalar_type entropy = 0;
        for (auto it=histogram_.cbegin(); it != histogram_.cend(); it++) {
            const scalar_type count = static_cast<scalar_type>(*it);
            if (count > 0) {
                const scalar_type relative_count = count / num_of_samples_;
                entropy -= relative_count * std::log2(relative_count);
            }
        }
        return entropy;
    }
    
#ifdef SERIALIZE_WITH_BOOST
    friend class boost::serialization::access;
#endif

    template <typename Archive>
    void serialize(Archive &archive, const unsigned int version)
    {
#ifdef SERIALIZE_WITH_BOOST
        archive & histogram_;
        archive & num_of_samples_;
#else
        archive(cereal::make_nvp("histogram", histogram_));
        archive(cereal::make_nvp("num_of_samples", num_of_samples_));
#endif
    }


//    template <typename Archive>
//    void save(Archive &archive, const unsigned int version) const
//    {
//        archive(cereal::make_nvp("histogram", histogram_));
//    }
//
//    template <typename Archive>
//    void load(Archive &archive, const unsigned int version)
//    {
//        archive(cereal::make_nvp("histogram", histogram_));
//        ComputeNumOfSamples();
//    }

private:
    void compute_num_of_samples()
    {
        num_of_samples_ = std::accumulate(histogram_.cbegin(), histogram_.cend(), 0);
    }

};

}
