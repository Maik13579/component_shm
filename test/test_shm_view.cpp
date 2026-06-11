// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "component_shm/shm_view.hpp"

namespace
{

class ShmViewTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    shm_ = std::make_shared<component_shm::SharedMemory>();
    view_ = std::make_unique<component_shm::ShmView>(shm_);
  }

  std::shared_ptr<component_shm::SharedMemory> shm_;
  std::unique_ptr<component_shm::ShmView> view_;
};

TEST_F(ShmViewTest, view_without_remapping_accesses_original_key)
{
  view_->set<int>("local", 10);

  EXPECT_TRUE(shm_->contains("local"));
  EXPECT_EQ(*shm_->get<int>("local"), 10);
}

TEST_F(ShmViewTest, view_remapping_redirects_set)
{
  view_->addRemapping("input_cloud", "slam/global_cloud");

  view_->set<std::string>("input_cloud", "points");

  EXPECT_TRUE(shm_->contains("slam/global_cloud"));
  EXPECT_FALSE(shm_->contains("input_cloud"));
  EXPECT_EQ(*shm_->get<std::string>("slam/global_cloud"), "points");
}

TEST_F(ShmViewTest, view_remapping_redirects_get)
{
  shm_->set<std::string>("slam/global_cloud", "points");
  view_->addRemapping("input_cloud", "slam/global_cloud");

  EXPECT_EQ(*view_->get<std::string>("input_cloud"), "points");
}

TEST_F(ShmViewTest, independent_views_have_independent_remappings)
{
  component_shm::ShmView view_a{shm_};
  component_shm::ShmView view_b{shm_};

  view_a.addRemapping("input", "slam/global");
  view_b.addRemapping("input", "perception/filtered");

  view_a.set<int>("input", 1);
  view_b.set<int>("input", 2);

  EXPECT_EQ(*shm_->get<int>("slam/global"), 1);
  EXPECT_EQ(*shm_->get<int>("perception/filtered"), 2);
  EXPECT_EQ(*view_a.get<int>("input"), 1);
  EXPECT_EQ(*view_b.get<int>("input"), 2);
}

TEST_F(ShmViewTest, remove_remapping_restores_original_key)
{
  view_->addRemapping("key", "mapped");
  view_->removeRemapping("key");

  EXPECT_FALSE(view_->hasRemapping("key"));
  EXPECT_EQ(view_->resolveKey("key"), "key");
}

TEST_F(ShmViewTest, set_remappings_replaces_existing_remappings)
{
  view_->addRemapping("old", "old/global");

  view_->set_remappings(
    std::unordered_map<std::string, std::string>{
      {"input", "slam/global"},
      {"output", "perception/filtered"}});

  EXPECT_FALSE(view_->hasRemapping("old"));
  EXPECT_EQ(view_->resolveKey("old"), "old");
  EXPECT_EQ(view_->resolveKey("input"), "slam/global");
  EXPECT_EQ(view_->resolveKey("output"), "perception/filtered");
}

TEST_F(ShmViewTest, get_remappings_returns_copy)
{
  view_->set_remappings(
    std::unordered_map<std::string, std::string>{
      {"input", "slam/global"},
      {"output", "perception/filtered"}});

  auto remappings = view_->get_remappings();
  remappings["input"] = "changed";

  EXPECT_EQ(remappings.size(), 2U);
  EXPECT_EQ(view_->resolveKey("input"), "slam/global");
  EXPECT_EQ(view_->get_remappings().at("output"), "perception/filtered");
}

TEST_F(ShmViewTest, set_remappings_empty_clears_existing_remappings)
{
  view_->set_remappings(
    std::unordered_map<std::string, std::string>{
      {"input", "slam/global"},
      {"output", "perception/filtered"}});

  view_->set_remappings({});

  EXPECT_TRUE(view_->get_remappings().empty());
  EXPECT_EQ(view_->resolveKey("input"), "input");
  EXPECT_EQ(view_->resolveKey("output"), "output");
}

TEST_F(ShmViewTest, view_contains_uses_remapping)
{
  shm_->set<int>("global", 4);
  view_->addRemapping("local", "global");

  EXPECT_TRUE(view_->contains("local"));
}

TEST_F(ShmViewTest, view_contains_typed_uses_remapping)
{
  shm_->set<int>("global", 4);
  view_->addRemapping("local", "global");

  EXPECT_TRUE(view_->containsTyped<int>("local"));
  EXPECT_FALSE(view_->containsTyped<double>("local"));
}

TEST_F(ShmViewTest, view_try_get_uses_remapping)
{
  shm_->set<int>("global", 4);
  view_->addRemapping("local", "global");

  const auto value = view_->tryGet<int>("local");

  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 4);
  EXPECT_EQ(view_->tryGet<double>("local"), nullptr);
}

TEST_F(ShmViewTest, view_remove_uses_remapping)
{
  shm_->set<int>("global", 4);
  view_->addRemapping("local", "global");

  view_->remove("local");

  EXPECT_FALSE(shm_->contains("global"));
}

TEST_F(ShmViewTest, set_shared_uses_resolved_key)
{
  view_->addRemapping("local", "global");
  auto value = std::make_shared<std::string>("payload");

  view_->setShared("local", value);

  EXPECT_EQ(shm_->get<std::string>("global"), value);
}

TEST_F(ShmViewTest, get_remappings_is_safe_while_set_remappings_runs)
{
  constexpr int iterations = 1000;
  constexpr int reader_count = 4;
  std::atomic<bool> valid_access{true};

  std::thread writer([&]() {
    for (int i = 0; i < iterations; ++i) {
      view_->set_remappings(
        std::unordered_map<std::string, std::string>{
          {"input", "global_" + std::to_string(i)},
          {"output", "filtered_" + std::to_string(i)}});
    }
  });

  std::vector<std::thread> readers;
  readers.reserve(reader_count);
  for (int i = 0; i < reader_count; ++i) {
    readers.emplace_back([&]() {
      for (int read = 0; read < iterations; ++read) {
        const auto remappings = view_->get_remappings();
        if (!remappings.empty() && remappings.size() != 2U) {
          valid_access.store(false);
        }
      }
    });
  }

  writer.join();
  for (auto & reader : readers) {
    reader.join();
  }

  EXPECT_TRUE(valid_access.load());
}

TEST(ShmViewConstructionTest, rejects_null_shared_memory)
{
  EXPECT_THROW(component_shm::ShmView{nullptr}, std::invalid_argument);
}

}  // namespace
