//
//  distributed_forest_trainer.h
//  DistRandomForest
//
//  Created by Benjamin Hepp on 30/09/15.
//
//

#pragma once

#include <iostream>
#include <sstream>
#include <map>

#include <boost/serialization/map.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/collectives.hpp>

#include "ait.h"
#include "level_forest_trainer.h"

namespace ait
{
    
namespace mpi = boost::mpi;
    
template <template <typename, typename> class TWeakLearner, typename TSampleIterator, typename TRandomEngine = std::mt19937_64>
class DistributedForestTrainer : public LevelForestTrainer<TWeakLearner, TSampleIterator, TRandomEngine>
{
    using BaseT = LevelForestTrainer<TWeakLearner, TSampleIterator, TRandomEngine>;

public:
    struct DistributedTrainingParameters : public BaseT::ParametersT
    {
        // TODO
    };

    using ParametersT = DistributedTrainingParameters;
    
    using SamplePointerT = typename BaseT::SamplePointerT;
    using WeakLearnerT = typename BaseT::WeakLearnerT;
    using SplitPointT = typename BaseT::SplitPointT;
    using StatisticsT = typename BaseT::StatisticsT;
    using TreeT = typename BaseT::TreeT;

    explicit DistributedForestTrainer(mpi::communicator &comm, const WeakLearnerT &weak_learner, const ParametersT &training_parameters)
    : BaseT(weak_learner, training_parameters), comm_(comm), training_parameters_(training_parameters)
    {}
    
    virtual ~DistributedForestTrainer()
    {}

protected:
    template <typename T> using TreeNodeMap = typename BaseT::template TreeNodeMap<T>;

    void broadcast_tree(TreeT &tree, int root = 0) const
    {
        if (comm_.rank() == root)
        {
            broadcast(comm_, tree, root);
        }
        else
        {
            TreeT bcast_tree;
            broadcast(comm_, bcast_tree, root);
            tree = std::move(bcast_tree);
        }
    }

    template <typename ValueType>
    void broadcast_tree_node_map(TreeT &tree, TreeNodeMap<ValueType> &map, int root = 0) const
    {
        std::map<size_type, ValueType> wrapper_map;
        if (comm_.rank() != root)
        {
            map.clear();
        }
        broadcast(comm_, map.base_map(), root);
    }
    
    void exchange_split_statistics_batch(TreeT &tree, TreeNodeMap<SplitStatistics<StatisticsT>> &map, int root = 0) const
    {
        std::vector<TreeNodeMap<SplitStatistics<StatisticsT>>> maps(comm_.size(), TreeNodeMap<SplitStatistics<StatisticsT>>(tree));
        gather(comm_, map, &maps[0], root);
        if (comm_.rank() == root)
        {
            log_debug() << "First split_statistics size: " << map.cbegin()->get_left_statistics(0).num_of_samples();
            map.clear();
            for (auto it = maps.cbegin(); it != maps.cend(); ++it)
            {
                for (auto map_it = it->cbegin(); map_it != it->cend(); ++map_it)
                {
                    SplitStatistics<StatisticsT> &split_statistics = map[map_it.node_iterator()];
                    const SplitStatistics<StatisticsT> &other_split_statistics = *map_it;
                    if (split_statistics.size() == 0)
                    {
                        split_statistics = other_split_statistics;
                    }
                    else
                    {
                        split_statistics.accumulate(other_split_statistics);
                    }
                }
            }
            log_debug() << "After accumulation: " << map.cbegin()->get_left_statistics(0).num_of_samples();
        }
        broadcast_tree_node_map(tree, map, root);
    }

    virtual TreeNodeMap<SplitStatistics<StatisticsT>> compute_split_statistics_batch(TreeT &tree, const TreeNodeMap<std::vector<SamplePointerT>> &node_to_sample_map, const TreeNodeMap<std::vector<SplitPointT>> &split_points_batch) const override
    {
        TreeNodeMap<SplitStatistics<StatisticsT>> split_statistics_batch = BaseT::compute_split_statistics_batch(tree, node_to_sample_map, split_points_batch);
        exchange_split_statistics_batch(tree, split_statistics_batch);
        return split_statistics_batch;
    }

    virtual TreeNodeMap<std::vector<SplitPointT>> sample_split_points_batch(TreeT &tree, const TreeNodeMap<std::vector<SamplePointerT>> &node_to_sample_map, TRandomEngine &rnd_engine) const override
    {
        TreeNodeMap<std::vector<SplitPointT>> split_points_batch(tree);
        if (comm_.rank() == 0)
        {
            split_points_batch = std::move(BaseT::sample_split_points_batch(tree, node_to_sample_map, rnd_engine));
        }
        broadcast_tree_node_map(tree, split_points_batch);
        return split_points_batch;
    }
    
    void exchange_statistics_batch(TreeT &tree, TreeNodeMap<StatisticsT> &map, int root = 0) const
    {
        std::vector<TreeNodeMap<StatisticsT>> maps(comm_.size(), TreeNodeMap<StatisticsT>(tree));
        gather(comm_, map, &maps[0], root);
        if (comm_.rank() == root)
        {
            log_debug() << "First statistics size: " << map.cbegin()->num_of_samples();
            map.clear();
            for (auto it = maps.cbegin(); it != maps.cend(); ++it)
            {
                log_debug() << "Processing statistics from rank " << (it - maps.cbegin());
                for (auto map_it = it->cbegin(); map_it != it->cend(); ++map_it)
                {
                    auto it = map.find(map_it.node_iterator());
                    if (it == map.end())
                    {
                        map[map_it.node_iterator()] = this->weak_learner_.create_statistics();
                    }
                    StatisticsT &statistics = map[map_it.node_iterator()];
//                    log_debug() << "Processing statistics for node " << map_it.node_iterator().get_node_index();
                    const StatisticsT &other_statistics = *map_it;
                    statistics.accumulate(other_statistics);
                }
            }
        }
        broadcast_tree_node_map(tree, map, root);
        // For debugging only:
//        if (comm_.rank() == root)
//        {
//            for (auto map_it = map.cbegin(); map_it != map.cend(); ++map_it)
//            {
//                auto logger = log_debug();
//                logger << "Num of samples for node [" << map_it.node_iterator().get_node_index() << "]: " << map_it->num_of_samples() << "{";
//                for (int i = 0; i < map_it->get_histogram().size(); ++i)
//                {
//                    logger << map_it->get_histogram()[i];
//                    if (i < map_it->get_histogram().size() - 1)
//                        logger << ", ";
//                }
//                logger << "}";
//            }
//        }

    }

    virtual TreeNodeMap<StatisticsT> compute_statistics_batch(TreeT &tree, TreeNodeMap<std::vector<SamplePointerT>> &node_to_sample_map) const override
    {
        TreeNodeMap<StatisticsT> statistics_batch = BaseT::compute_statistics_batch(tree, node_to_sample_map);
        exchange_statistics_batch(tree, statistics_batch);
        return statistics_batch;
    }

    virtual void train_tree_level(TreeT &tree, size_type current_level, TSampleIterator samples_start, TSampleIterator samples_end, TRandomEngine &rnd_engine) const override
    {
        BaseT::train_tree_level(tree, current_level, samples_start, samples_end, rnd_engine);
        broadcast_tree(tree);
    }

private:
    mpi::communicator comm_;
    ParametersT training_parameters_;
};
    
}
