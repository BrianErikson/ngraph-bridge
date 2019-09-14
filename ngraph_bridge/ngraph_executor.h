/*******************************************************************************
 * Copyright 2019 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#ifndef NGRAPH_EXECUTOR_H_
#define NGRAPH_EXECUTOR_H_
#pragma once

#include <ostream>
#include <vector>

#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/graph/graph.h"

#include "ngraph/ngraph.hpp"

#include "logging/ngraph_log.h"
#include "ngraph_bridge/ngraph_freshness_tracker.h"
#include "ngraph_bridge/ngraph_pipelined_tensors.h"

namespace tensorflow {

namespace ngraph_bridge {

using NgFunctionIOCache = std::unordered_map<
    std::shared_ptr<ngraph::runtime::Executable>,
    std::vector<std::pair<void*, shared_ptr<ng::runtime::Tensor>>>>;

class NGraphExecutor {
 public:
  // TODO
  //  Try to remove the following members and pass them through the constructor
  //  SetNgraphCluster()
  //    - Also, rename the corresponding data member to m_ckluster_id;
  //  SetGraphId()
  //  Investigate whether the static input map can be created within the c-tor
  //  of the
  //    NGraphExecutor()
  //  SetOpBackend()
  //    This can easily be passed via the ctor
  //  SetExecCanCreateTensor()
  //
  // Ngraph Encapsulate Implementation class for EncapsulateOp class
  explicit NGraphExecutor(int instance_id, unique_ptr<tensorflow::Graph>& graph,
                          const string& backend_name);

  // Calls Compute Signature and gets ngraph executable
  // Update the cache and if called again with the same input shapes,
  // return fromm the cache
  Status GetNgExecutable(const std::vector<Tensor>& tf_input_tensors,
                         std::vector<TensorShape>& input_shapes,
                         std::vector<const Tensor*>& static_input_map,
                         ng::runtime::Backend*& op_backend,
                         std::shared_ptr<ngraph::runtime::Executable>& ng_exec,
                         bool& cache_hit);

  const string& GetOpBackend() { return m_op_backend_name; }
  bool GetExecCanCreateTensor() { return m_executable_can_create_tensor; }

  // TODO Rename this to DecodeAttributes
  Status ParseNodeAttributes(
      const google::protobuf::Map<string, AttrValue>& additional_attributes,
      std::unordered_map<std::string, std::string>* additional_attribute_map);

  std::tuple<int, PipelinedTensorVector, PipelinedTensorVector>
  GetTensorsFromPipeline(std::shared_ptr<ngraph::runtime::Executable> ng_exec);

 private:
  // Get tensorflow input tensors, input shapes, static_inputs to Compute
  // Signature
  Status ComputeSignature(const std::vector<Tensor>& tf_input_tensors,
                          std::vector<TensorShape>& input_shapes,
                          std::vector<const Tensor*>& static_input_map,
                          std::stringstream& signature_ss);

  // Allocate tensors for input arguments. Creates ngraph input tensors using
  // tensorflow tensors required to execute ngraph function
  Status AllocateNGInputTensors(
      const std::vector<Tensor>& tf_input_tensors,
      const std::shared_ptr<ngraph::runtime::Executable>& ng_exec,
      const PipelinedTensorVector& inp_group_from_pipeline,
      ng::runtime::Backend* const op_backend,
      vector<shared_ptr<ng::runtime::Tensor>>& ng_inputs);

  // Allocate tensors for output results.  Creates ngraph output tensors using
  // tensorflow tensors required to execute ngraph function
  Status AllocateNGOutputTensors(
      const std::vector<Tensor*>& tf_output_tensors,
      const std::shared_ptr<ngraph::runtime::Executable>& ng_exec,
      const PipelinedTensorVector& out_group_from_pipeline,
      ng::runtime::Backend* const op_backend,
      vector<shared_ptr<ng::runtime::Tensor>>& ng_outputs);

  // Get current ngraph tensor
  std::shared_ptr<ng::runtime::Tensor> GetCurrentNgTensor(
      void* current_tf_ptr, void* last_tf_ptr,
      const std::shared_ptr<ng::runtime::Tensor>& last_ng_tensor,
      const bool& output_tensor,
      const std::shared_ptr<ngraph::runtime::Executable>& ng_exec,
      ng::runtime::Backend* const op_backend,
      const ng::element::Type& ng_element_type, const ng::Shape& ng_shape,
      std::shared_ptr<ng::runtime::Tensor> tensor_from_pipeline);

  // Accessors(getters and setters) for the private data members of
  // NGraphExecutor class
  // needed by
  // NgraphEncapsulateOp class
  const int& GetNumberOfCopies() { return number_of_copies; }

  void SetNumberOfCopies(const int& number) { number_of_copies = number; }

  const int& GetNgraphCluster() { return m_ngraph_cluster; }

  void SetNgraphCluster(const int& cluster) { m_ngraph_cluster = cluster; }

  int GetGraphId() { return m_graph_id; }

  void SetGraphId(const int& graph_id) { m_graph_id = graph_id; }

  const int& GetFunctionCache() { return my_function_cache_depth_in_items; }

  const int& GetNumberOfOutputs() { return m_number_outputs; }

  void SetNumberOfOutputs(const int& n) { m_number_outputs = n; }

  const int& GetNumberOfInputs() { return m_number_inputs; }

  void SetNumberOfInputs(const int& n) { m_number_inputs = n; }

  const int& GetInstanceId() { return my_instance_id; }

  bool GetLogCopies() { return log_copies; }

  void SetLogCopies(bool value) { log_copies = value; }

  const string GetCopyLog() { return copy_log_str.str(); }

  void SetCopyLog(const string str) { copy_log_str.str() = str; }

  const std::vector<bool> GetStaticInputVector() { return m_input_is_static; }

 public:
  void ResizeStaticInputVector(const int& size) {
    m_input_is_static.resize(size);
  }
  void SetStaticInputVector(const int& index, bool value) {
    m_input_is_static[index] = value;
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<ngraph::runtime::Executable>>
  GetNgExecMap() {
    return m_ng_exec_map;
  }

  void SetNgExecMap(const std::string& ng_map_key,
                    const std::shared_ptr<ngraph::runtime::Executable>& exec) {
    m_ng_exec_map[ng_map_key] = exec;
  }

  void ClearNgExecMap() { m_ng_exec_map.clear(); }

  std::unordered_map<std::shared_ptr<ngraph::runtime::Executable>,
                     std::shared_ptr<ngraph::Function>>
  GetNgFunctionMap() {
    return m_ng_function_map;
  }

  void SetNgFunctionMap(
      const std::shared_ptr<ngraph::runtime::Executable>& exec,
      const std::shared_ptr<ngraph::Function>& function) {
    m_ng_function_map[exec] = function;
  }

  void ClearNgFunctionMap() { m_ng_function_map.clear(); }
  // TODO:sindhu have another get function for output_cache which is only
  // readable
  std::vector<std::pair<void*, shared_ptr<ng::runtime::Tensor>>>&
  GetNgExecOutputCacheMap(std::shared_ptr<ngraph::runtime::Executable> exec) {
    return m_ng_exec_output_cache_map[exec];
  }

  void SetNgExecOutputCacheMap(
      const std::shared_ptr<ngraph::runtime::Executable>& exec,
      const std::vector<std::pair<void*, shared_ptr<ng::runtime::Tensor>>>&
          cache) {
    m_ng_exec_output_cache_map[exec] = cache;
  }

  void ClearNgExecInputCache() { m_ng_exec_input_cache_map.clear(); }

  void ClearNgExecOutputCache() { m_ng_exec_output_cache_map.clear(); }

  NGraphFreshnessTracker* GetNgraphFreshnessTracker() {
    return m_freshness_tracker;
  }

  void SetNgraphFreshnessTracker(NGraphFreshnessTracker* tracker) {
    m_freshness_tracker = tracker;
  }

  void SetName(string name) { m_name = name; }

  void ClearNgExecPipelinedTensorMap() {
    m_executable_pipelined_tensors_map.clear();
  }

  public:
  Status CachePipelinedTensorIfNeeded(
      std::shared_ptr<ngraph::runtime::Executable> ng_exec);
  private:

  void ReturnPipelinedTensors(
      std::shared_ptr<ngraph::runtime::Executable> ng_exec, size_t idx) {
    m_executable_pipelined_tensors_map.at(ng_exec).return_tensors(idx);
  }

  // TF Graph for the cluster
  const unique_ptr<Graph> m_graph;

 private:
  int number_of_copies = 0;
  int m_ngraph_cluster{-1};
  int m_graph_id{-1};
  int my_function_cache_depth_in_items = 16;
  int m_number_outputs = -1;
  int m_number_inputs = -1;
  const int my_instance_id;
  const string m_op_backend_name;
  string m_name;
  std::stringstream copy_log_str;
  bool log_copies = false;
  std::vector<bool> m_input_is_static;
  std::list<std::string> m_lru;
  bool m_do_aot = false;
  map<string, string> m_aot_functions;
  map<string, string> m_aot_execs;

  // ng_function, ng_executable, Output and Input Cache maps
  std::unordered_map<std::string, std::shared_ptr<ngraph::runtime::Executable>>
      m_ng_exec_map;
  std::unordered_map<std::shared_ptr<ngraph::runtime::Executable>,
                     std::shared_ptr<ngraph::Function>>
      m_ng_function_map;

  NgFunctionIOCache m_ng_exec_input_cache_map;
  NgFunctionIOCache m_ng_exec_output_cache_map;

  // Freshness tracker maintains a set of ng::functions using a particular base
  // pointer(for Tensor)
  // A single instance of freshness_tracker is used across all
  // nGraphEncapsulateOp and nGraphVariable op
  NGraphFreshnessTracker* m_freshness_tracker;

  bool m_executable_can_create_tensor;
  std::unordered_map<std::shared_ptr<ngraph::runtime::Executable>,
                     PipelinedTensorsStore>
      m_executable_pipelined_tensors_map;

  int m_depth{2};  // TODO make this settable

  // Synchronization objects
  // 1. Protect the functioncache
};

}  // namespace ngraph_bridge

}  // namespace tensorflow
#endif  // NGRAPH_EXECUTOR_H_
