// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "component_shm/shared_memory.hpp"

namespace
{

class SharedMemoryTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    shm_ = std::make_shared<component_shm::SharedMemory>();
    shm_->clear();
  }

  std::shared_ptr<component_shm::SharedMemory> shm_;
};

TEST_F(SharedMemoryTest, set_and_get_value)
{
  shm_->set<int>("value", 42);

  const auto value = shm_->get<int>("value");

  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 42);
}

TEST_F(SharedMemoryTest, overwrite_value)
{
  shm_->set<int>("count", 1);
  shm_->set<int>("count", 2);

  EXPECT_EQ(*shm_->get<int>("count"), 2);
}

TEST_F(SharedMemoryTest, missing_key_throws)
{
  EXPECT_THROW(shm_->get<int>("missing"), std::out_of_range);
}

TEST_F(SharedMemoryTest, wrong_type_throws)
{
  shm_->set<int>("answer", 42);

  EXPECT_THROW(shm_->get<double>("answer"), std::bad_cast);
}

TEST_F(SharedMemoryTest, try_get_missing_returns_nullptr)
{
  EXPECT_EQ(shm_->tryGet<int>("missing"), nullptr);
}

TEST_F(SharedMemoryTest, try_get_wrong_type_returns_nullptr)
{
  shm_->set<int>("answer", 42);

  EXPECT_EQ(shm_->tryGet<double>("answer"), nullptr);
}

TEST_F(SharedMemoryTest, contains_detects_key)
{
  shm_->set<int>("value", 1);

  EXPECT_TRUE(shm_->contains("value"));
}

TEST_F(SharedMemoryTest, contains_typed_detects_correct_type)
{
  shm_->set<int>("value", 1);

  EXPECT_TRUE(shm_->containsTyped<int>("value"));
  EXPECT_FALSE(shm_->containsTyped<double>("value"));
}

TEST_F(SharedMemoryTest, remove_deletes_key)
{
  shm_->set<int>("value", 1);

  shm_->remove("value");

  EXPECT_FALSE(shm_->contains("value"));
}

TEST_F(SharedMemoryTest, clear_deletes_all_keys)
{
  shm_->set<int>("a", 1);
  shm_->set<int>("b", 2);

  shm_->clear();

  EXPECT_FALSE(shm_->contains("a"));
  EXPECT_FALSE(shm_->contains("b"));
}

TEST_F(SharedMemoryTest, set_shared_preserves_ownership)
{
  auto value = std::make_shared<std::vector<int>>(std::initializer_list<int>{1, 2, 3});

  shm_->setShared("numbers", value);
  auto stored = shm_->get<std::vector<int>>("numbers");

  EXPECT_EQ(stored, value);
  EXPECT_EQ(stored->size(), 3U);
}

TEST_F(SharedMemoryTest, set_shared_rejects_nullptr)
{
  EXPECT_THROW(shm_->setShared<int>("null", nullptr), std::invalid_argument);
}

TEST_F(SharedMemoryTest, multiple_readers_single_writer)
{
  constexpr int iterations = 2000;
  constexpr int reader_count = 4;
  std::atomic<bool> valid_access{true};

  std::thread writer([&]() {
    for (int i = 0; i < iterations; ++i) {
      shm_->set<int>("value", i);
    }
  });

  std::vector<std::thread> readers;
  readers.reserve(reader_count);
  for (int i = 0; i < reader_count; ++i) {
    readers.emplace_back([&]() {
      for (int read = 0; read < iterations; ++read) {
        const auto value = shm_->tryGet<int>("value");
        if (value && (*value < 0 || *value >= iterations)) {
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

TEST_F(SharedMemoryTest, parallel_contains_and_set)
{
  constexpr int iterations = 2000;
  constexpr int reader_count = 4;
  std::atomic<bool> valid_access{true};

  std::thread writer([&]() {
    for (int i = 0; i < iterations; ++i) {
      shm_->set<int>("value", i);
    }
  });

  std::vector<std::thread> readers;
  readers.reserve(reader_count);
  for (int i = 0; i < reader_count; ++i) {
    readers.emplace_back([&]() {
      for (int read = 0; read < iterations; ++read) {
        if (shm_->contains("value")) {
          if (!shm_->containsTyped<int>("value") || shm_->containsTyped<double>("value")) {
            valid_access.store(false);
          }
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

TEST(SharedMemorySingletonTest, instance_returns_process_global_registry)
{
  const auto first = component_shm::SharedMemory::instance();
  const auto second = component_shm::SharedMemory::instance();
  first->clear();

  first->set<int>("shared", 5);

  EXPECT_EQ(first, second);
  EXPECT_EQ(*second->get<int>("shared"), 5);

  first->clear();
}

}  // namespace
