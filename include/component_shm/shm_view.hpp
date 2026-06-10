// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#ifndef COMPONENT_SHM__SHM_VIEW_HPP_
#define COMPONENT_SHM__SHM_VIEW_HPP_

#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "component_shm/shared_memory.hpp"

namespace component_shm
{

class ShmView
{
public:
  explicit ShmView(std::shared_ptr<SharedMemory> shm = SharedMemory::instance());

  void addRemapping(const std::string & from, const std::string & to);
  void removeRemapping(const std::string & from);
  bool hasRemapping(const std::string & key) const;
  std::string resolveKey(const std::string & key) const;

  template<typename T>
  void set(const std::string & key, T value)
  {
    shm_->set<T>(resolveKey(key), std::move(value));
  }

  template<typename T>
  void setShared(const std::string & key, std::shared_ptr<T> value)
  {
    shm_->setShared<T>(resolveKey(key), std::move(value));
  }

  template<typename T>
  std::shared_ptr<T> get(const std::string & key) const
  {
    return shm_->get<T>(resolveKey(key));
  }

  template<typename T>
  std::shared_ptr<T> tryGet(const std::string & key) const
  {
    return shm_->tryGet<T>(resolveKey(key));
  }

  bool contains(const std::string & key) const;

  template<typename T>
  bool containsTyped(const std::string & key) const
  {
    return shm_->containsTyped<T>(resolveKey(key));
  }

  void remove(const std::string & key);

private:
  std::shared_ptr<SharedMemory> shm_;
  std::unordered_map<std::string, std::string> remappings_;
  mutable std::shared_mutex remapping_mutex_;
};

}  // namespace component_shm

#endif  // COMPONENT_SHM__SHM_VIEW_HPP_
