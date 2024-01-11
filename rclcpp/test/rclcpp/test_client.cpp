// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#include <gtest/gtest.h>

#include <string>
#include <memory>
#include <utility>

#include "rclcpp/exceptions.hpp"
#include "rclcpp/rclcpp.hpp"

#include "rcl_interfaces/srv/list_parameters.hpp"

#include "../mocking_utils/patch.hpp"
#include "../utils/rclcpp_gtest_macros.hpp"

#include "test_client_common.hpp"

#include "test_msgs/srv/empty.hpp"

class TestClient : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    rclcpp::init(0, nullptr);
  }

  static void TearDownTestCase()
  {
    rclcpp::shutdown();
  }

  void SetUp()
  {
    node = std::make_shared<rclcpp::Node>("my_node", "/ns");
  }

  void TearDown()
  {
    node.reset();
  }

  rclcpp::Node::SharedPtr node;
};

class TestClientSub : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    rclcpp::init(0, nullptr);
  }

  static void TearDownTestCase()
  {
    rclcpp::shutdown();
  }

  void SetUp()
  {
    node = std::make_shared<rclcpp::Node>("my_node", "/ns");
    subnode = node->create_sub_node("sub_ns");
  }

  void TearDown()
  {
    node.reset();
  }

  rclcpp::Node::SharedPtr node;
  rclcpp::Node::SharedPtr subnode;
};

/*
   Testing client construction and destruction.
 */
TEST_F(TestClient, construction_and_destruction) {
  using rcl_interfaces::srv::ListParameters;
  {
    auto client = node->create_client<ListParameters>("service");
  }
  {
    // suppress deprecated function warning
    #if !defined(_WIN32)
    # pragma GCC diagnostic push
    # pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #else  // !defined(_WIN32)
    # pragma warning(push)
    # pragma warning(disable: 4996)
    #endif

    auto client = node->create_client<ListParameters>(
      "service", rmw_qos_profile_services_default);

    // remove warning suppression
    #if !defined(_WIN32)
    # pragma GCC diagnostic pop
    #else  // !defined(_WIN32)
    # pragma warning(pop)
    #endif
  }
  {
    auto client = node->create_client<ListParameters>(
      "service", rclcpp::ServicesQoS());
  }

  {
    ASSERT_THROW(
    {
      auto client = node->create_client<ListParameters>("invalid_service?");
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
}

TEST_F(TestClient, construction_with_free_function) {
  {
    auto client = rclcpp::create_client<rcl_interfaces::srv::ListParameters>(
      node->get_node_base_interface(),
      node->get_node_graph_interface(),
      node->get_node_services_interface(),
      "service",
      rmw_qos_profile_services_default,
      nullptr);
  }
  {
    ASSERT_THROW(
    {
      auto client = rclcpp::create_client<rcl_interfaces::srv::ListParameters>(
        node->get_node_base_interface(),
        node->get_node_graph_interface(),
        node->get_node_services_interface(),
        "invalid_?service",
        rmw_qos_profile_services_default,
        nullptr);
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
  {
    auto client = rclcpp::create_client<rcl_interfaces::srv::ListParameters>(
      node->get_node_base_interface(),
      node->get_node_graph_interface(),
      node->get_node_services_interface(),
      "service",
      rclcpp::ServicesQoS(),
      nullptr);
  }
  {
    ASSERT_THROW(
    {
      auto client = rclcpp::create_client<rcl_interfaces::srv::ListParameters>(
        node->get_node_base_interface(),
        node->get_node_graph_interface(),
        node->get_node_services_interface(),
        "invalid_?service",
        rclcpp::ServicesQoS(),
        nullptr);
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
}

TEST_F(TestClient, construct_with_rcl_error) {
  {
    // reset() is not necessary for this exception, but handles unused return value warning
    auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_client_init, RCL_RET_ERROR);
    EXPECT_THROW(
      node->create_client<test_msgs::srv::Empty>("service").reset(),
      rclcpp::exceptions::RCLError);
  }
  {
    // reset() is required for this one
    auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_client_fini, RCL_RET_ERROR);
    EXPECT_NO_THROW(node->create_client<test_msgs::srv::Empty>("service").reset());
  }
}

TEST_F(TestClient, wait_for_service) {
  const std::string service_name = "service";
  test_client_common::wait_for_service<rclcpp::Client<test_msgs::srv::Empty>>(node, service_name);
}

/*
   Testing client construction and destruction for subnodes.
 */
TEST_F(TestClientSub, construction_and_destruction) {
  using rcl_interfaces::srv::ListParameters;
  {
    auto client = subnode->create_client<ListParameters>("service");
    EXPECT_STREQ(client->get_service_name(), "/ns/sub_ns/service");
  }

  {
    ASSERT_THROW(
    {
      auto client = node->create_client<ListParameters>("invalid_service?");
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
}

class TestClientWithServer : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    rclcpp::init(0, nullptr);
  }

  static void TearDownTestCase()
  {
    rclcpp::shutdown();
  }

  void SetUp()
  {
    node = std::make_shared<rclcpp::Node>("node", "ns");

    auto callback = [](
      const test_msgs::srv::Empty::Request::SharedPtr,
      test_msgs::srv::Empty::Response::SharedPtr) {};

    service = node->create_service<test_msgs::srv::Empty>(service_name, std::move(callback));
  }

  ::testing::AssertionResult SendEmptyRequestAndWait(
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
  {
    using SharedFuture = rclcpp::Client<test_msgs::srv::Empty>::SharedFuture;

    auto client = node->create_client<test_msgs::srv::Empty>(service_name);
    if (!client->wait_for_service()) {
      return ::testing::AssertionFailure() << "Waiting for service failed";
    }

    auto request = std::make_shared<test_msgs::srv::Empty::Request>();
    bool received_response = false;
    ::testing::AssertionResult request_result = ::testing::AssertionSuccess();
    auto callback = [&received_response, &request_result](SharedFuture future_response) {
        if (nullptr == future_response.get()) {
          request_result = ::testing::AssertionFailure() << "Future response was null";
        }
        received_response = true;
      };

    auto req_id = client->async_send_request(request, std::move(callback));

    auto start = std::chrono::steady_clock::now();
    while (!received_response &&
      (std::chrono::steady_clock::now() - start) < timeout)
    {
      rclcpp::spin_some(node);
    }

    if (!received_response) {
      return ::testing::AssertionFailure() << "Waiting for response timed out";
    }
    if (client->remove_pending_request(req_id)) {
      return ::testing::AssertionFailure() << "Should not be able to remove a finished request";
    }

    return request_result;
  }

  std::shared_ptr<rclcpp::Node> node;
  std::shared_ptr<rclcpp::Service<test_msgs::srv::Empty>> service;
  const std::string service_name{"empty_service"};
};

TEST_F(TestClientWithServer, async_send_request) {
  EXPECT_TRUE(SendEmptyRequestAndWait());
}

TEST_F(TestClientWithServer, async_send_request_callback_with_request) {
  using SharedFutureWithRequest =
    rclcpp::Client<test_msgs::srv::Empty>::SharedFutureWithRequest;

  auto client = node->create_client<test_msgs::srv::Empty>(service_name);
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(1)));

  auto request = std::make_shared<test_msgs::srv::Empty::Request>();
  bool received_response = false;
  auto callback = [&request, &received_response](SharedFutureWithRequest future) {
      auto request_response_pair = future.get();
      EXPECT_EQ(request, request_response_pair.first);
      EXPECT_NE(nullptr, request_response_pair.second);
      received_response = true;
    };
  auto req_id = client->async_send_request(request, std::move(callback));

  auto start = std::chrono::steady_clock::now();
  while (!received_response &&
    (std::chrono::steady_clock::now() - start) < std::chrono::seconds(1))
  {
    rclcpp::spin_some(node);
  }
  EXPECT_TRUE(received_response);
  EXPECT_FALSE(client->remove_pending_request(req_id));
}

TEST_F(TestClientWithServer, test_client_remove_pending_request) {
  test_client_common::test_client_remove_pending_request<rclcpp::Client<test_msgs::srv::Empty>>(
    node, service_name);
}

TEST_F(TestClientWithServer, prune_requests_older_than_no_pruned) {
  test_client_common::prune_requests_older_than_no_pruned<rclcpp::Client<test_msgs::srv::Empty>>(
    node, service_name);
}

TEST_F(TestClientWithServer, prune_requests_older_than_with_pruned) {
  test_client_common::prune_requests_older_than_with_pruned<rclcpp::Client<test_msgs::srv::Empty>>(
    node, service_name);
}

TEST_F(TestClientWithServer, async_send_request_rcl_send_request_error) {
  // Checking rcl_send_request in rclcpp::Client::async_send_request()
  auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_send_request, RCL_RET_ERROR);
  EXPECT_THROW(SendEmptyRequestAndWait(), rclcpp::exceptions::RCLError);
}

TEST_F(TestClientWithServer, async_send_request_rcl_service_server_is_available_error) {
  test_client_common::async_send_request_rcl_service_server_is_available_error<
    rclcpp::Client<test_msgs::srv::Empty>>(node, service_name);
}

TEST_F(TestClientWithServer, take_response) {
  test_client_common::take_response<rclcpp::Client<test_msgs::srv::Empty>>(node, service_name);
}

/*
   Testing on_new_response callbacks.
 */
TEST_F(TestClient, on_new_response_callback) {
  test_client_common::on_new_response_callback<rclcpp::Client<test_msgs::srv::Empty>>();
}

TEST_F(TestClient, rcl_client_request_publisher_get_actual_qos_error) {
  auto mock = mocking_utils::patch_and_return(
    "lib:rclcpp", rcl_client_request_publisher_get_actual_qos, nullptr);
  auto client = node->create_client<test_msgs::srv::Empty>("service");
  RCLCPP_EXPECT_THROW_EQ(
    client->get_request_publisher_actual_qos(),
    std::runtime_error("failed to get client's request publisher qos settings: error not set"));
}

TEST_F(TestClient, rcl_client_response_subscription_get_actual_qos_error) {
  auto mock = mocking_utils::patch_and_return(
    "lib:rclcpp", rcl_client_response_subscription_get_actual_qos, nullptr);
  auto client = node->create_client<test_msgs::srv::Empty>("service");
  RCLCPP_EXPECT_THROW_EQ(
    client->get_response_subscription_actual_qos(),
    std::runtime_error("failed to get client's response subscription qos settings: error not set"));
}

TEST_F(TestClient, client_qos) {
  test_client_common::client_qos<rclcpp::GenericClient>(node, "client");
}

TEST_F(TestClient, client_qos_depth) {
  using namespace std::literals::chrono_literals;

  rclcpp::ServicesQoS client_qos_profile;
  client_qos_profile.keep_last(2);

  auto client = node->create_client<test_msgs::srv::Empty>("test_qos_depth", client_qos_profile);

  uint64_t server_cb_count_ = 0;
  auto server_callback = [&](
    const test_msgs::srv::Empty::Request::SharedPtr,
    test_msgs::srv::Empty::Response::SharedPtr) {server_cb_count_++;};

  auto server_node = std::make_shared<rclcpp::Node>("server_node", "/ns");

  rclcpp::QoS server_qos(rclcpp::QoSInitialization::from_rmw(rmw_qos_profile_default));

  auto server = server_node->create_service<test_msgs::srv::Empty>(
    "test_qos_depth", std::move(server_callback), server_qos);

  auto request = std::make_shared<test_msgs::srv::Empty::Request>();
  ::testing::AssertionResult request_result = ::testing::AssertionSuccess();

  using SharedFuture = rclcpp::Client<test_msgs::srv::Empty>::SharedFuture;
  uint64_t client_cb_count_ = 0;
  auto client_callback = [&client_cb_count_, &request_result](SharedFuture future_response) {
      if (nullptr == future_response.get()) {
        request_result = ::testing::AssertionFailure() << "Future response was null";
      }
      client_cb_count_++;
    };

  uint64_t client_requests = 5;
  for (uint64_t i = 0; i < client_requests; i++) {
    client->async_send_request(request, client_callback);
    std::this_thread::sleep_for(10ms);
  }

  auto start = std::chrono::steady_clock::now();
  while ((server_cb_count_ < client_requests) &&
    (std::chrono::steady_clock::now() - start) < 2s)
  {
    rclcpp::spin_some(server_node);
    std::this_thread::sleep_for(2ms);
  }

  EXPECT_GT(server_cb_count_, client_qos_profile.depth());

  start = std::chrono::steady_clock::now();
  while ((client_cb_count_ < client_qos_profile.depth()) &&
    (std::chrono::steady_clock::now() - start) < 1s)
  {
    rclcpp::spin_some(node);
  }

  // Spin an extra time to check if client QoS depth has been ignored,
  // so more client callbacks might be called than expected.
  rclcpp::spin_some(node);

  EXPECT_EQ(client_cb_count_, client_qos_profile.depth());
}
