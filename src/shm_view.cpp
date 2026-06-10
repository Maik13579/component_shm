// Copyright 2026 Maik Knof
// SPDX-License-Identifier: Apache-2.0

#include "component_shm/shm_view.hpp"

#include <mutex>

namespace component_shm
{

ShmView::ShmView(std::shared_ptr<SharedMemory> shm)
: shm_(std::move(shm))
{
  if (!shm_) {
    throw std::invalid_argument("component_shm::ShmView requires a SharedMemory instance");
  }
}

void ShmView::addRemapping(const std::string & from, const std::string & to)
{
  std::unique_lock<std::shared_mutex> lock(remapping_mutex_);
  remappings_[from] = to;
}

void ShmView::removeRemapping(const std::string & from)
{
  std::unique_lock<std::shared_mutex> lock(remapping_mutex_);
  remappings_.erase(from);
}

bool ShmView::hasRemapping(const std::string & key) const
{
  std::shared_lock<std::shared_mutex> lock(remapping_mutex_);
  return remappings_.find(key) != remappings_.end();
}

std::string ShmView::resolveKey(const std::string & key) const
{
  std::shared_lock<std::shared_mutex> lock(remapping_mutex_);
  const auto it = remappings_.find(key);
  if (it != remappings_.end()) {
    return it->second;
  }
  return key;
}

bool ShmView::contains(const std::string & key) const
{
  return shm_->contains(resolveKey(key));
}

void ShmView::remove(const std::string & key)
{
  shm_->remove(resolveKey(key));
}

}  // namespace component_shm
