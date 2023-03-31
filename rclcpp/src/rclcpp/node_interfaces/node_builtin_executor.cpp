// Copyright 2023 Sony Group Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rclcpp/node_interfaces/node_builtin_executor.hpp"
#include "rclcpp/executors/multi_threaded_executor.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rclcpp/node_interfaces/node_logging.hpp"

using rclcpp::node_interfaces::NodeBuiltinExecutor;

NodeBuiltinExecutor::NodeBuiltinExecutor(
  node_interfaces::NodeBaseInterface::SharedPtr node_base,
  node_interfaces::NodeTopicsInterface::SharedPtr node_topics,
  node_interfaces::NodeServicesInterface::SharedPtr node_services,
  node_interfaces::NodeLoggingInterface::SharedPtr node_logging,
  const NodeOptions & node_options)
: node_base_(node_base),
  node_topics_(node_topics),
  node_services_(node_services),
  node_logging_(node_logging)
{
  bool run_builtin_performance_callback_group = false;
  bool run_builtin_callback_group = false;
  int thread_num = 0;

  if (node_options.enable_logger_service()) {
    dynamic_cast<node_interfaces::NodeLogging *>(node_logging_.get())->add_logger_service(
      node_base_, node_services_);
    run_builtin_callback_group = true;
  }

  if (node_options.use_clock_thread()) {
    run_builtin_performance_callback_group = true;
  }

  thread_num =
    (run_builtin_performance_callback_group ? 1 : 0) + (run_builtin_callback_group ? 1 : 0);

  if (thread_num > 0) {
    ExecutorOptions executor_option;
    executor_option.context = node_options.context();

    if (thread_num == 1) {
      executor_ = std::make_shared<executors::SingleThreadedExecutor>(executor_option);
    } else {
      executor_ = std::make_shared<executors::MultiThreadedExecutor>(
        executor_option,
        thread_num
      );
    }

    if (run_builtin_performance_callback_group) {
      executor_->add_callback_group(
        node_base_->get_builtin_performance_callback_group(),
        node_base_);
    }

    if (run_builtin_callback_group) {
      executor_->add_callback_group(
        node_base_->get_builtin_callback_group(),
        node_base_);
    }

    executor_promise_ = std::promise<void>{};
    thread_ = std::thread(
      [this]() {
        auto future = executor_promise_.get_future();
        executor_->spin_until_future_complete(future);
      }
    );
  }
}

NodeBuiltinExecutor::~NodeBuiltinExecutor()
{
  if (thread_.joinable()) {
    executor_promise_.set_value();
    executor_->cancel();
    thread_.join();
  }
}

bool NodeBuiltinExecutor::builtin_thread_joinable()
{
  return thread_.joinable();
}
