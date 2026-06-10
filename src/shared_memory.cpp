// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#include "component_shm/shared_memory.hpp"

namespace component_shm
{

std::shared_ptr<SharedMemory> SharedMemory::instance()
{
  static std::shared_ptr<SharedMemory> instance{new SharedMemory()};
  return instance;
}

bool SharedMemory::contains(const std::string & key) const
{
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return storage_.find(key) != storage_.end();
}

void SharedMemory::remove(const std::string & key)
{
  std::unique_lock<std::shared_mutex> lock(mutex_);
  storage_.erase(key);
}

void SharedMemory::clear()
{
  std::unique_lock<std::shared_mutex> lock(mutex_);
  storage_.clear();
}

}  // namespace component_shm
