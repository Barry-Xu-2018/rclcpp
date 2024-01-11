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

#include <gtest/gtest.h>

#include <string>
#include <memory>
#include <utility>

#include "rclcpp/create_generic_client.hpp"
#include "rclcpp/exceptions.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"

#include "rcl_interfaces/srv/list_parameters.hpp"

#include "../mocking_utils/patch.hpp"
#include "../utils/rclcpp_gtest_macros.hpp"

#include "test_client_common.hpp"

#include "test_msgs/srv/empty.hpp"

using namespace std::chrono_literals;

// All tests are from test_client

class TestGenericClient : public ::testing::Test
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
    node = std::make_shared<rclcpp::Node>("test_node", "/ns");
  }

  void TearDown()
  {
    node.reset();
  }

  rclcpp::Node::SharedPtr node;
};

class TestGenericClientSub : public ::testing::Test
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
    node = std::make_shared<rclcpp::Node>("test_node", "/ns");
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
TEST_F(TestGenericClient, construction_and_destruction) {
  {
    auto client = node->create_generic_client("test_service", "test_msgs/srv/Empty");
  }

  {
    ASSERT_THROW(
    {
      auto client = node->create_generic_client("invalid_test_service?", "test_msgs/srv/Empty");
    }, rclcpp::exceptions::InvalidServiceNameError);
  }

  {
    ASSERT_THROW(
    {
      auto client = node->create_generic_client("test_service", "test_msgs/srv/InvalidType");
    }, std::runtime_error);
  }
}

TEST_F(TestGenericClient, construction_with_free_function) {
  {
    auto client = rclcpp::create_generic_client(
      node->get_node_base_interface(),
      node->get_node_graph_interface(),
      node->get_node_services_interface(),
      "test_service",
      "test_msgs/srv/Empty",
      rclcpp::ServicesQoS(),
      nullptr);
  }
  {
    ASSERT_THROW(
    {
      auto client = rclcpp::create_generic_client(
        node->get_node_base_interface(),
        node->get_node_graph_interface(),
        node->get_node_services_interface(),
        "invalid_?test_service",
        "test_msgs/srv/Empty",
        rclcpp::ServicesQoS(),
        nullptr);
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
  {
    ASSERT_THROW(
    {
      auto client = rclcpp::create_generic_client(
        node->get_node_base_interface(),
        node->get_node_graph_interface(),
        node->get_node_services_interface(),
        "test_service",
        "test_msgs/srv/InvalidType",
        rclcpp::ServicesQoS(),
        nullptr);
    }, std::runtime_error);
  }
  {
    auto client = rclcpp::create_generic_client(
      node,
      "test_service",
      "test_msgs/srv/Empty",
      rclcpp::ServicesQoS(),
      nullptr);
  }
  {
    ASSERT_THROW(
    {
      auto client = rclcpp::create_generic_client(
        node,
        "invalid_?test_service",
        "test_msgs/srv/Empty",
        rclcpp::ServicesQoS(),
        nullptr);
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
  {
    ASSERT_THROW(
    {  
      auto client = rclcpp::create_generic_client(
        node,
        "invalid_?test_service",
        "test_msgs/srv/InvalidType",
        rclcpp::ServicesQoS(),
        nullptr);
    }, std::runtime_error);
  }
}

TEST_F(TestGenericClient, construct_with_rcl_error) {
  {
    // reset() is not necessary for this exception, but handles unused return value warning
    auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_client_init, RCL_RET_ERROR);
    EXPECT_THROW(
      node->create_generic_client("test_service", "test_msgs/srv/Empty").reset(),
      rclcpp::exceptions::RCLError);
  }
  {
    // reset() is required for this one
    auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_client_fini, RCL_RET_ERROR);
    EXPECT_NO_THROW(
      node->create_generic_client("test_service", "test_msgs/srv/Empty").reset());
  }
}

TEST_F(TestGenericClient, wait_for_service) {
  const std::string service_name = "test_service";

  test_client_common::wait_for_service<rclcpp::GenericClient>(node, service_name);
}

/*
   Testing generic client construction and destruction for subnodes.
 */
TEST_F(TestGenericClientSub, construction_and_destruction) {
  {
    auto client = subnode->create_generic_client("test_service", "test_msgs/srv/Empty");
    EXPECT_STREQ(client->get_service_name(), "/ns/test_service");
  }

  {
    ASSERT_THROW(
    {
      auto client = node->create_generic_client("invalid_service?", "test_msgs/srv/Empty");
    }, rclcpp::exceptions::InvalidServiceNameError);
  }
}

class TestGenericClientWithServer : public ::testing::Test
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
    node = std::make_shared<rclcpp::Node>("test_node", "ns");

    auto callback = [](
      const test_msgs::srv::Empty::Request::SharedPtr,
      test_msgs::srv::Empty::Response::SharedPtr) {};

    service = node->create_service<test_msgs::srv::Empty>(service_name, std::move(callback));
  }

  ::testing::AssertionResult SendEmptyRequestAndWait(
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
  {
    auto client = node->create_generic_client(service_name, "test_msgs/srv/Empty");
    if (!client->wait_for_service()) {
      return ::testing::AssertionFailure() << "Service is not available yet";
    }

    auto request = std::make_shared<test_msgs::srv::Empty::Request>();

    auto future_and_req_id = client->async_send_request(request.get());

    auto ret = rclcpp::spin_until_future_complete(node, future_and_req_id, timeout);
    if (ret != rclcpp::FutureReturnCode::SUCCESS) {
      return ::testing::AssertionFailure() << "Waiting for response timed out";
    }

    if (client->remove_pending_request(future_and_req_id.request_id)) {
      return ::testing::AssertionFailure() << "Should not be able to remove a finished request";
    }

    return ::testing::AssertionSuccess();
  }

  std::shared_ptr<rclcpp::Node> node;
  std::shared_ptr<rclcpp::Service<test_msgs::srv::Empty>> service;
  const std::string service_name{"empty_service"};
};

TEST_F(TestGenericClientWithServer, async_send_request) {
  EXPECT_TRUE(SendEmptyRequestAndWait());
}

TEST_F(TestGenericClientWithServer, test_client_remove_pending_request) {
  test_client_common::test_client_remove_pending_request<rclcpp::GenericClient>(
    node, "no_service_server_available_here");
}

TEST_F(TestGenericClientWithServer, prune_requests_older_than_no_pruned) {
  test_client_common::prune_requests_older_than_no_pruned<rclcpp::GenericClient>(
    node, service_name);
}

TEST_F(TestGenericClientWithServer, prune_requests_older_than_with_pruned) {
  test_client_common::prune_requests_older_than_with_pruned<rclcpp::GenericClient>(
    node, service_name);
}

TEST_F(TestGenericClientWithServer, async_send_request_rcl_send_request_error) {
  // Checking rcl_send_request in rclcpp::Client::async_send_request()
  auto mock = mocking_utils::patch_and_return("lib:rclcpp", rcl_send_request, RCL_RET_ERROR);
  EXPECT_THROW(SendEmptyRequestAndWait(), rclcpp::exceptions::RCLError);
}

TEST_F(TestGenericClientWithServer, async_send_request_rcl_service_server_is_available_error) {
  test_client_common::async_send_request_rcl_service_server_is_available_error<
    rclcpp::GenericClient>(node, service_name);
}

TEST_F(TestGenericClientWithServer, take_response) {
  test_client_common::take_response<rclcpp::GenericClient>(node, service_name);
}

/*
   Testing on_new_response callbacks.
 */
TEST_F(TestGenericClient, on_new_response_callback) {
  test_client_common::on_new_response_callback<rclcpp::GenericClient>();
}

TEST_F(TestGenericClient, client_qos) {
  test_client_common::client_qos<rclcpp::GenericClient>(node, "test_client");
}
