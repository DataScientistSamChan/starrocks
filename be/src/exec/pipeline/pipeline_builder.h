// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <exec/pipeline/scan/olap_scan_operator.h>

#include "exec/pipeline/exchange/local_exchange.h"
#include "exec/pipeline/exchange/local_exchange_sink_operator.h"
#include "exec/pipeline/exchange/local_exchange_source_operator.h"
#include "exec/pipeline/fragment_context.h"
#include "exec/pipeline/pipeline.h"
#include "exec/pipeline/spill_process_channel.h"

namespace starrocks {
class ExecNode;
namespace pipeline {

class PipelineBuilderContext {
public:
    PipelineBuilderContext(FragmentContext* fragment_context, size_t degree_of_parallelism, bool is_stream_pipeline)
            : _fragment_context(fragment_context),
              _degree_of_parallelism(degree_of_parallelism),
              _is_stream_pipeline(is_stream_pipeline) {}

    void add_pipeline(const OpFactories& operators) {
        _pipelines.emplace_back(std::make_shared<Pipeline>(next_pipe_id(), operators));
    }

    OpFactories maybe_interpolate_local_broadcast_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                           int num_receivers);

    // Input the output chunks from the drivers of pred operators into ONE driver of the post operators.
    OpFactories maybe_interpolate_local_passthrough_exchange(RuntimeState* state, OpFactories& pred_operators);
    OpFactories maybe_interpolate_local_passthrough_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                             int num_receivers, bool force = false);
    OpFactories maybe_interpolate_local_random_passthrough_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                                    int num_receivers, bool force = false);
    OpFactories maybe_interpolate_local_adpative_passthrough_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                                      int num_receivers, bool force = false);

    /// Local shuffle the output chunks from multiple drivers of pred operators into DOP partitions of the post operators.
    /// The partition is generated by evaluated each row via partition_expr_ctxs.
    /// When interpolating a local shuffle?
    /// - Local shuffle is interpolated only when DOP > 1 and the source operator of pred_operators could local shuffle.
    /// partition_exprs
    /// - If the source operator has a partition exprs, use it as partition_exprs.
    /// - Otherwise, use self_partition_exprs or self_partition_exprs_generator().
    OpFactories maybe_interpolate_local_shuffle_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                         const std::vector<ExprContext*>& self_partition_exprs);
    using PartitionExprsGenerator = std::function<std::vector<ExprContext*>()>;
    OpFactories maybe_interpolate_local_shuffle_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                         const PartitionExprsGenerator& self_partition_exprs_generator);

    void interpolate_spill_process(size_t plan_node_id, const SpillProcessChannelFactoryPtr& channel_factory,
                                   size_t dop);

    // Uses local exchange to gather the output chunks of multiple predecessor pipelines
    // into a new pipeline, which the successor operator belongs to.
    // Append a LocalExchangeSinkOperator to the tail of each pipeline.
    // Create a new pipeline with a LocalExchangeSourceOperator.
    // These local exchange sink operators and the source operator share a passthrough exchanger.
    OpFactories maybe_gather_pipelines_to_one(RuntimeState* state, std::vector<OpFactories>& pred_operators_list);

    OpFactories maybe_interpolate_collect_stats(RuntimeState* state, OpFactories& pred_operators);

    uint32_t next_pipe_id() { return _next_pipeline_id++; }

    uint32_t next_operator_id() { return _next_operator_id++; }

    int32_t next_pseudo_plan_node_id() { return _next_pseudo_plan_node_id--; }

    size_t degree_of_parallelism() const { return _degree_of_parallelism; }

    bool is_stream_pipeline() const { return _is_stream_pipeline; }

    const Pipelines& get_pipelines() const { return _pipelines; }
    const Pipeline* last_pipeline() const {
        DCHECK(!_pipelines.empty());
        return _pipelines[_pipelines.size() - 1].get();
    }

    RuntimeState* runtime_state() { return _fragment_context->runtime_state(); }
    FragmentContext* fragment_context() { return _fragment_context; }

    size_t dop_of_source_operator(int source_node_id);
    MorselQueueFactory* morsel_queue_factory_of_source_operator(int source_node_id);
    MorselQueueFactory* morsel_queue_factory_of_source_operator(const SourceOperatorFactory* source_op);
    SourceOperatorFactory* source_operator(OpFactories ops);
    // Whether the building pipeline `ops` need local shuffle for the next operator.
    bool could_local_shuffle(OpFactories ops) const;

    bool should_interpolate_cache_operator(OpFactoryPtr& source_op, int32_t plan_node_id);
    OpFactories interpolate_cache_operator(
            OpFactories& upstream_pipeline, OpFactories& downstream_pipeline,
            const std::function<std::tuple<OpFactoryPtr, SourceOperatorFactoryPtr>(bool)>& merge_operators_generator);

    // help to change some actions after aggregations, for example,
    // disable to ignore local data after aggregations with profile exchange speed.
    bool has_aggregation = false;

    static int localExchangeBufferChunks() { return kLocalExchangeBufferChunks; }

    void inherit_upstream_source_properties(SourceOperatorFactory* downstream_source,
                                            SourceOperatorFactory* upstream_source);

    void push_dependent_pipeline(const Pipeline* pipeline);
    void pop_dependent_pipeline();

    bool force_disable_adaptive_dop() const { return _force_disable_adaptive_dop; }
    void set_force_disable_adaptive_dop(bool val) { _force_disable_adaptive_dop = val; }

private:
    OpFactories _maybe_interpolate_local_passthrough_exchange(RuntimeState* state, OpFactories& pred_operators,
                                                              int num_receivers, bool force,
                                                              LocalExchanger::PassThroughType pass_through_type);

    OpFactories _do_maybe_interpolate_local_shuffle_exchange(
            RuntimeState* state, OpFactories& pred_operators, const std::vector<ExprContext*>& partition_expr_ctxs,
            const TPartitionType::type part_type = TPartitionType::type::HASH_PARTITIONED);

    static constexpr int kLocalExchangeBufferChunks = 8;

    FragmentContext* _fragment_context;
    Pipelines _pipelines;

    std::list<const Pipeline*> _dependent_pipelines;

    uint32_t _next_pipeline_id = 0;
    uint32_t _next_operator_id = 0;
    int32_t _next_pseudo_plan_node_id = Operator::s_pseudo_plan_node_id_upper_bound;

    const size_t _degree_of_parallelism;

    const bool _is_stream_pipeline;

    bool _force_disable_adaptive_dop = false;
};

class PipelineBuilder {
public:
    PipelineBuilder(PipelineBuilderContext& context) : _context(context) {}

    // Build pipeline from exec node tree
    Pipelines build(const FragmentContext& fragment, ExecNode* exec_node);

private:
    PipelineBuilderContext& _context;
};
} // namespace pipeline
} // namespace starrocks
