// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#ifndef COMPONENT_SHM__SHARED_MEMORY_HPP_
#define COMPONENT_SHM__SHARED_MEMORY_HPP_

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace component_shm
{

/// @brief Thread-safe, type-erased data registry for ROS 2 components in one process.
class SharedMemory
{
public:
  /// @brief Return the process-global shared registry instance.
  static std::shared_ptr<SharedMemory> instance();

  /// @brief Store value under key, replacing any existing entry at that key.
  template<typename T>
  void set(const std::string & key, T value)
  {
    using StoredType = std::decay_t<T>;

    std::unique_lock<std::shared_mutex> lock(mutex_);
    storage_.insert_or_assign(
      key,
      Entry{
        std::make_shared<StoredType>(std::move(value)),
        std::type_index(typeid(StoredType))});
  }

  /// @brief Store a non-null shared object under key while preserving shared ownership.
  template<typename T>
  void setShared(const std::string & key, std::shared_ptr<T> value)
  {
    if (!value) {
      throw std::invalid_argument("component_shm::SharedMemory::setShared received nullptr");
    }

    using StoredType = std::remove_cv_t<T>;

    std::unique_lock<std::shared_mutex> lock(mutex_);
    storage_.insert_or_assign(
      key,
      Entry{std::static_pointer_cast<void>(value), std::type_index(typeid(StoredType))});
  }

  /// @brief Get the value stored under key, throwing for missing keys or type mismatches.
  template<typename T>
  std::shared_ptr<T> get(const std::string & key) const
  {
    using StoredType = std::remove_cv_t<T>;

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = storage_.find(key);
    if (it == storage_.end()) {
      throw std::out_of_range("component_shm key not found: " + key);
    }
    if (it->second.type != std::type_index(typeid(StoredType))) {
      throw std::bad_cast();
    }
    return std::static_pointer_cast<T>(it->second.data);
  }

  /// @brief Get the value stored under key, or nullptr for missing keys or type mismatches.
  template<typename T>
  std::shared_ptr<T> tryGet(const std::string & key) const
  {
    using StoredType = std::remove_cv_t<T>;

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = storage_.find(key);
    if (it == storage_.end() || it->second.type != std::type_index(typeid(StoredType))) {
      return nullptr;
    }
    return std::static_pointer_cast<T>(it->second.data);
  }

  /// @brief Return true if any entry exists at key.
  bool contains(const std::string & key) const;

  /// @brief Return true if key exists and the stored type exactly matches T.
  template<typename T>
  bool containsTyped(const std::string & key) const
  {
    using StoredType = std::remove_cv_t<T>;

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = storage_.find(key);
    return it != storage_.end() && it->second.type == std::type_index(typeid(StoredType));
  }

  /// @brief Remove key if it exists.
  void remove(const std::string & key);

  /// @brief Remove every entry from the registry.
  void clear();

private:
  /// @brief Type-erased storage plus runtime type metadata used for checked casts.
  struct Entry
  {
    std::shared_ptr<void> data;
    std::type_index type;
  };

  std::unordered_map<std::string, Entry> storage_;
  mutable std::shared_mutex mutex_;
};

}  // namespace component_shm

#endif  // COMPONENT_SHM__SHARED_MEMORY_HPP_
