#pragma once

#include <iostream>
#include <sstream>

#include "forest.h"
#include "weak_learner.h"

namespace ait
{

template <typename TSample, typename TWeakLearner, typename TTrainingParameters, typename TRandomEngine>
class DistributedForestTrainer
{
public:
    typedef typename TWeakLearner::Statistics Statistics;
    typedef typename TWeakLearner::SampleIterator SampleIterator;
    typedef typename TWeakLearner::_SplitPoint SplitPoint;
    typedef typename TWeakLearner::_SplitPointCollection SplitPointCollection;
    typedef typename AIT::Tree<SplitPoint, Statistics>::NodeIterator NodeIterator;

private:
    const TWeakLearner weak_learner_;
    const TTrainingParameters training_parameters_;

public:
    DistributedForestTrainer(const TWeakLearner &weak_learner, const TTrainingParameters &training_parameters)
    : weak_learner_(weak_learner), training_parameters_(training_parameters)
    {}

    void TrainTreeLevel(NodeIterator root_iter, SampleIterator i_start, SampleIterator i_end, TRandomEngine &rnd_engine, int current_depth = 1) const
    {
        std::cout << "depth: " << current_depth << std::endl;

        // stop splitting the node if the minimum number of samples has been reached
        if (i_end - i_start < training_parameters_.MinimumNumOfSamples()) {
            //node.leaf_node = True
            output_spaces(std::cout, current_depth - 1);
            std::cout << "Minimum number of samples. Stopping." << std::endl;
            return;
        }
        
        // stop splitting the node if it is a leaf node
        if (node_iter.IsLeafNode()) {
            output_spaces(std::cout, current_depth - 1);
            std::cout << "Reached leaf node. Stopping." << std::endl;
            return;
        }
        
        SplitPointCollection split_points = weak_learner_.SampleSplitPoints(i_start, i_end, training_parameters_.NumOfFeatures(), training_parameters_.NumOfThresholds(), rnd_engine);
        
        // TODO: distribute features and thresholds to ranks > 0
        
        // compute the statistics for all feature and threshold combinations
        SplitStatistics<typename TWeakLearner::Statistics> split_statistics = weak_learner_.ComputeSplitStatistics(i_start, i_end, split_points);
        
        // TODO: send statistics to rank 0
        // send split_statistics.get_buffer()
        
        // TODO: receive statistics from rank > 0
        // for received statistics
        // split_statistics.accumulate(statistics)
        
        // find the best feature(only on rank 0)
        std::tuple<typename TWeakLearner::size_type, typename TWeakLearner::size_type, typename TWeakLearner::entropy_type> best_split_point_tuple = weak_learner_.FindBestSplitPoint(statistics, split_statistics);
        
        // TODO: send best feature, threshold and information gain to ranks > 0
        
        typename TWeakLearner::entropy_type best_information_gain = std::get<2>(best_split_point_tuple);
        // TODO: move criterion into trainingContext ?
        // stop splitting the node if the best information gain is below the minimum information gain
        if (best_information_gain < training_parameters_.MinimumInformationGain()) {
            //node.leaf_node = True
            output_spaces(std::cout, current_depth - 1);
            std::cout << "Too little information gain. Stopping." << std::endl;
            return;
        }
        
        // partition sample_indices according to the selected feature and threshold.
        // i.e.sample_indices[:i_split] will contain the left child indices
        // and sample_indices[i_split:] will contain the right child indices
        typename TWeakLearner::size_type best_feature_index = std::get<0>(best_split_point_tuple);
        typename TWeakLearner::size_type best_threshold_index = std::get<1>(best_split_point_tuple);
        typename TWeakLearner::_SplitPoint best_split_point = split_points.GetSplitPoint(best_feature_index, best_threshold_index);
        typename TWeakLearner::SampleIterator i_split = weak_learner_.Partition(i_start, i_end, best_split_point);
        
        node_iter->SetSplitPoint(best_split_point);

        // TODO: can we reuse computed statistics from split_point_context ? ? ?
        //left_child_statistics = None
        //right_child_statistics = None

    }
    
    Tree<SplitPoint, Statistics> TrainTree(std::vector<TSample> &samples) const
    {
        TRandomEngine rnd_engine;
        return TrainTree(samples, rnd_engine);
    }
    
    
    Tree<SplitPoint, Statistics> TrainTree(std::vector<TSample> &samples, TRandomEngine &rnd_engine) const
    {
        Tree<SplitPoint, Statistics> tree(training_parameters_.TreeDepth());
        for (size_type current_depth = 1; current_depth <= training_parameters_.TreeDepth(); current_depth++) {
            // TODO: pass const sample iterators?
            TrainTreeLevel(tree.GetRoot(), samples.begin(), samples.end(), rnd_engine);
        }
        return tree;
    }

    Forest<SplitPoint, Statistics> TrainForest(std::vector<TSample> &samples) const
    {
        TRandomEngine rnd_engine;
        return TrainForest(samples, rnd_engine);
    }
    
    Forest<SplitPoint, Statistics> TrainForest(std::vector<TSample> &samples, TRandomEngine &rnd_engine) const
    {
        Forest<SplitPoint, Statistics> forest;
        for (int i=0; i < training_parameters_.NumOfTrees(); i++) {
            Tree<SplitPoint, Statistics> tree = TrainTree(samples, rnd_engine);
            forest.AddTree(std::move(tree));
        }
        return forest;
    }
    
};
    
}
