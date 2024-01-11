// Copyright 2024 Sony Group Corporation.
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

#ifndef RCLCPP__TEST_CLIENT_COMMON_HPP_
#define RCLCPP__TEST_CLIENT_COMMON_HPP_

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../mocking_utils/patch.hpp"

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"

#include "test_msgs/srv/empty.hpp"

namespace test_client_common
{
// The common tests for Client and GenericClient

#define DECLARE_CREATE_CLIENT(node, service_name) \
  auto call_create_client = [&]() { \
      if constexpr (std::is_same<Client, rclcpp::GenericClient::SharedPtr>::value) { \
        return node->create_generic_client(service_name, "test_msgs/srv/Empty"); \
      } else { \
        return node->template create_client<test_msgs::srv::Empty>(service_name); \
      } \
    }

#define DECLARE_CALL_ASYNC_SEND_REQUEST(request) \
  auto call_async_send_request = [&]() { \
      if constexpr (std::is_same<Client, rclcpp::GenericClient::SharedPtr>::value) { \
        return client->async_send_request(request.get()); \
      } else { \
        return client->async_send_request(request); \
      } \
    }

#define DECLARE_CALL_TAKE_RESPONSE(response, request_header) \
  auto call_take_response = [&]() { \
      if constexpr (std::is_same<Client, rclcpp::GenericClient::SharedPtr>::value) { \
        return client->take_response(static_cast<void *>(&response), *request_header.get()); \
      } else { \
        return client->take_response(response, *request_header.get()); \
      } \
    }


template<typename Client>
void wait_for_service(rclcpp::Node::SharedPtr node, const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);

  auto client = call_create_client();

  EXPECT_FALSE(client->wait_for_service(std::chrono::nanoseconds(0)));
  EXPECT_FALSE(client->wait_for_service(std::chrono::milliseconds(10)));

  auto callback = [](
    const test_msgs::srv::Empty::Request::SharedPtr,
    test_msgs::srv::Empty::Response::SharedPtr) {};

  auto service =
    node->template create_service<test_msgs::srv::Empty>(service_name, std::move(callback));

  EXPECT_TRUE(client->wait_for_service(std::chrono::nanoseconds(-1)));
  EXPECT_TRUE(client->service_is_ready());
}

template<typename Client>
void test_client_remove_pending_request(
  rclcpp::Node::SharedPtr node,
  const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);

  auto client = call_create_client();

  auto request = std::make_shared<test_msgs::srv::Empty::Request>();

  DECLARE_CALL_ASYNC_SEND_REQUEST(request);

  auto future_and_req_id = call_async_send_request();
  EXPECT_TRUE(client->remove_pending_request(future_and_req_id.request_id));
}

template<typename Client>
void prune_requests_older_than_no_pruned(
  rclcpp::Node::SharedPtr node,
  const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);

  auto client = call_create_client();

  auto request = std::make_shared<test_msgs::srv::Empty::Request>();

  DECLARE_CALL_ASYNC_SEND_REQUEST(request);

  auto future = call_async_send_request();
  auto time = std::chrono::system_clock::now() + std::chrono::seconds(1);

  EXPECT_EQ(1u, client->prune_requests_older_than(time));
}

template<typename Client>
void prune_requests_older_than_with_pruned(
  rclcpp::Node::SharedPtr node,
  const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);

  auto client = call_create_client();

  auto request = std::make_shared<test_msgs::srv::Empty::Request>();

  DECLARE_CALL_ASYNC_SEND_REQUEST(request);

  auto future = call_async_send_request();
  auto time = std::chrono::system_clock::now() + std::chrono::seconds(1);

  std::vector<int64_t> pruned_requests;
  EXPECT_EQ(1u, client->prune_requests_older_than(time, &pruned_requests));
  ASSERT_EQ(1u, pruned_requests.size());
  EXPECT_EQ(future.request_id, pruned_requests[0]);
}

template<typename Client>
void async_send_request_rcl_service_server_is_available_error(
  rclcpp::Node::SharedPtr node,
  const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);
  {
    // Checking rcl_service_server_is_available in rclcpp::ClientBase::service_is_ready
    auto client = call_create_client();
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_service_server_is_available, RCL_RET_NODE_INVALID);
    EXPECT_THROW(client->service_is_ready(), rclcpp::exceptions::RCLError);
  }
  {
    // Checking rcl_service_server_is_available exception in rclcpp::ClientBase::service_is_ready
    auto client = call_create_client();
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_service_server_is_available, RCL_RET_ERROR);
    EXPECT_THROW(client->service_is_ready(), rclcpp::exceptions::RCLError);
  }
  {
    // Checking rcl_service_server_is_available exception in rclcpp::ClientBase::service_is_ready
    auto client = call_create_client();
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_service_server_is_available, RCL_RET_ERROR);
    EXPECT_THROW(client->service_is_ready(), rclcpp::exceptions::RCLError);
  }
}

template<typename Client>
void take_response(rclcpp::Node::SharedPtr node, const std::string & service_name)
{
  DECLARE_CREATE_CLIENT(node, service_name);

  auto client = call_create_client();
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(1)));
  auto request = std::make_shared<test_msgs::srv::Empty::Request>();
  auto request_header = client->create_request_header();
  test_msgs::srv::Empty::Response response;

  DECLARE_CALL_ASYNC_SEND_REQUEST(request);
  call_async_send_request();

  DECLARE_CALL_TAKE_RESPONSE(response, request_header);
  EXPECT_FALSE(call_take_response());

  {
    // Checking rcl_take_response in rclcpp::ClientBase::take_type_erased_response
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_take_response, RCL_RET_OK);
    EXPECT_TRUE(call_take_response());
  }
  {
    // Checking rcl_take_response in rclcpp::ClientBase::take_type_erased_response
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_take_response, RCL_RET_CLIENT_TAKE_FAILED);
    EXPECT_FALSE(call_take_response());
  }
  {
    // Checking rcl_take_response in rclcpp::ClientBase::take_type_erased_response
    auto mock = mocking_utils::patch_and_return(
      "lib:rclcpp", rcl_take_response, RCL_RET_ERROR);
    EXPECT_THROW(
      call_take_response(),
      rclcpp::exceptions::RCLError);
  }
}

#define DECLARE_CREATE_CLIENT_WITH_QOS(node, service_name, qos) \
  auto call_create_client = [&]() { \
      if constexpr (std::is_same<Client, rclcpp::GenericClient::SharedPtr>::value) { \
        return node->create_generic_client(service_name, "test_msgs/srv/Empty", qos); \
      } else { \
        return node->template create_client<test_msgs::srv::Empty>(service_name, qos); \
      } \
    }

template<typename Client>
void on_new_response_callback()
{
  auto client_node = std::make_shared<rclcpp::Node>("test_client_node", "ns");
  auto server_node = std::make_shared<rclcpp::Node>("test_server_node", "ns");

  rclcpp::ServicesQoS client_qos;
  client_qos.keep_last(3);

  DECLARE_CREATE_CLIENT_WITH_QOS(client_node, "test_service", client_qos);
  auto client = call_create_client();

  std::atomic<size_t> server_requests_count {0};
  auto server_callback = [&server_requests_count](
    const test_msgs::srv::Empty::Request::SharedPtr,
    test_msgs::srv::Empty::Response::SharedPtr) {server_requests_count++;};
  auto server = server_node->create_service<test_msgs::srv::Empty>(
    "test_service", server_callback, client_qos);
  auto request = std::make_shared<test_msgs::srv::Empty::Request>();

  std::atomic<size_t> c1 {0};
  auto increase_c1_cb = [&c1](size_t count_msgs) {c1 += count_msgs;};
  client->set_on_new_response_callback(increase_c1_cb);

  DECLARE_CALL_ASYNC_SEND_REQUEST(request);
  call_async_send_request();
  auto start = std::chrono::steady_clock::now();
  while (server_requests_count == 0 &&
    (std::chrono::steady_clock::now() - start) < std::chrono::seconds(10))
  {
    rclcpp::spin_some(server_node);
  }

  ASSERT_EQ(server_requests_count, 1u);

  start = std::chrono::steady_clock::now();
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (c1 == 0 && std::chrono::steady_clock::now() - start < std::chrono::seconds(10));

  EXPECT_EQ(c1.load(), 1u);

  std::atomic<size_t> c2 {0};
  auto increase_c2_cb = [&c2](size_t count_msgs) {c2 += count_msgs;};
  client->set_on_new_response_callback(increase_c2_cb);

  call_async_send_request();
  start = std::chrono::steady_clock::now();
  while (server_requests_count == 1 &&
    (std::chrono::steady_clock::now() - start) < std::chrono::seconds(10))
  {
    rclcpp::spin_some(server_node);
  }

  ASSERT_EQ(server_requests_count, 2u);

  start = std::chrono::steady_clock::now();
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (c1 == 0 && std::chrono::steady_clock::now() - start < std::chrono::seconds(10));

  EXPECT_EQ(c1.load(), 1u);
  EXPECT_EQ(c2.load(), 1u);

  client->clear_on_new_response_callback();

  call_async_send_request();
  call_async_send_request();
  call_async_send_request();
  start = std::chrono::steady_clock::now();
  while (server_requests_count < 5 &&
    (std::chrono::steady_clock::now() - start) < std::chrono::seconds(10))
  {
    rclcpp::spin_some(server_node);
  }

  ASSERT_EQ(server_requests_count, 5u);

  std::atomic<size_t> c3 {0};
  auto increase_c3_cb = [&c3](size_t count_msgs) {c3 += count_msgs;};
  client->set_on_new_response_callback(increase_c3_cb);

  start = std::chrono::steady_clock::now();
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (c3 < 3 && std::chrono::steady_clock::now() - start < std::chrono::seconds(10));

  EXPECT_EQ(c1.load(), 1u);
  EXPECT_EQ(c2.load(), 1u);
  EXPECT_EQ(c3.load(), 3u);

  std::function<void(size_t)> invalid_cb = nullptr;
  EXPECT_THROW(client->set_on_new_response_callback(invalid_cb), std::invalid_argument);
}

template<typename Client>
void client_qos(rclcpp::Node::SharedPtr node, const std::string & service_name)
{
  rclcpp::ServicesQoS qos_profile;
  qos_profile.liveliness(rclcpp::LivelinessPolicy::Automatic);
  rclcpp::Duration duration(std::chrono::nanoseconds(1));
  qos_profile.deadline(duration);
  qos_profile.lifespan(duration);
  qos_profile.liveliness_lease_duration(duration);

  DECLARE_CREATE_CLIENT_WITH_QOS(node, service_name, qos_profile);
  auto client = call_create_client();

  auto rp_qos = client->get_request_publisher_actual_qos();
  auto rs_qos = client->get_response_subscription_actual_qos();

  EXPECT_EQ(qos_profile, rp_qos);
  // Lifespan has no meaning for subscription/readers
  rs_qos.lifespan(qos_profile.lifespan());
  EXPECT_EQ(qos_profile, rs_qos);
}

}  // namespace test_client_common

#endif  // RCLCPP__TEST_CLIENT_COMMON_HPP_
