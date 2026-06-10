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

/// @brief Per-component access view that resolves local keys before registry access.
class ShmView
{
public:
  /// @brief Create a view over shm, defaulting to the process-global SharedMemory.
  explicit ShmView(std::shared_ptr<SharedMemory> shm = SharedMemory::instance());

  /// @brief Add or replace a remapping from a local key to a shared registry key.
  void addRemapping(const std::string & from, const std::string & to);

  /// @brief Remove a remapping if it exists.
  void removeRemapping(const std::string & from);

  /// @brief Return true if key has a local remapping in this view.
  bool hasRemapping(const std::string & key) const;

  /// @brief Return the remapped key, or key unchanged when no remapping exists.
  std::string resolveKey(const std::string & key) const;

  /// @brief Store value at the resolved key.
  template<typename T>
  void set(const std::string & key, T value)
  {
    shm_->set<T>(resolveKey(key), std::move(value));
  }

  /// @brief Store a non-null shared object at the resolved key.
  template<typename T>
  void setShared(const std::string & key, std::shared_ptr<T> value)
  {
    shm_->setShared<T>(resolveKey(key), std::move(value));
  }

  /// @brief Get a value from the resolved key.
  template<typename T>
  std::shared_ptr<T> get(const std::string & key) const
  {
    return shm_->get<T>(resolveKey(key));
  }

  /// @brief Try to get a value from the resolved key.
  template<typename T>
  std::shared_ptr<T> tryGet(const std::string & key) const
  {
    return shm_->tryGet<T>(resolveKey(key));
  }

  /// @brief Return true if any entry exists at the resolved key.
  bool contains(const std::string & key) const;

  /// @brief Return true if the resolved key exists and stores exactly type T.
  template<typename T>
  bool containsTyped(const std::string & key) const
  {
    return shm_->containsTyped<T>(resolveKey(key));
  }

  /// @brief Remove the entry at the resolved key if it exists.
  void remove(const std::string & key);

private:
  std::shared_ptr<SharedMemory> shm_;
  std::unordered_map<std::string, std::string> remappings_;
  mutable std::shared_mutex remapping_mutex_;
};

}  // namespace component_shm

#endif  // COMPONENT_SHM__SHM_VIEW_HPP_
