# component_shm

`component_shm` is a lightweight, thread-safe, type-erased shared data registry
for ROS 2 composable nodes running in the same process. It gives components a
typed API while storing values internally as `std::shared_ptr<void>` with
`std::type_index` metadata.

## In-process only

This package does not provide inter-process shared memory. Data is shared only
between components loaded into the same process. Components in different
processes, containers, or machines need ROS topics, services, actions, or a
separate inter-process shared-memory transport.

## Minimal usage

`component_shm::SharedMemory` is the process-global registry:

```cpp
#include "component_shm/shared_memory.hpp"

auto shm = component_shm::SharedMemory::instance();

shm->set<std::string>("status", "ready");
auto status = shm->get<std::string>("status");
```

`SharedMemory::instance()` returns the same shared registry object to every
caller in the process. Reads use `std::shared_lock<std::shared_mutex>` and
writes use `std::unique_lock<std::shared_mutex>`.

## ShmView remapping

`component_shm::ShmView` adds per-component key remapping:

```cpp
#include "component_shm/shm_view.hpp"

component_shm::ShmView view;
view.addRemapping("input_cloud", "slam/global_cloud");

view.set<int>("input_cloud", 42);
auto value = view.get<int>("input_cloud");
```

For a point-cloud component, a local key can map to a shared global key:

```cpp
component_shm::ShmView view;

view.addRemapping("input_cloud", "slam/global_cloud");

auto cloud = view.get<pcl::PointCloud<pcl::PointXYZ>>("input_cloud");
```

Remappings are local to the view. Different components can resolve the same
local key to different global keys while sharing the same `SharedMemory`
instance.

## Thread safety

`SharedMemory` uses a `std::shared_mutex` around the global registry. Read-only
operations use shared locks so multiple readers can run concurrently. Modifying
operations use exclusive locks so `set()`, `setShared()`, `remove()`, and
`clear()` cannot race each other.

`ShmView` has its own `std::shared_mutex` for remappings. It resolves the local
key, releases the remapping lock, and then calls `SharedMemory`, which manages
the data lock.

## Limitations

- In-process only; no cross-process or cross-machine sharing.
- Values must be retrieved with the exact stored C++ type.
- No serialization, history, QoS, or ROS graph integration.
- No automatic cleanup by component lifecycle; remove or clear keys explicitly
  when ownership semantics require it.
- Stored objects are shared by pointer, so callers must still coordinate
  mutation of the object contents if they modify a retrieved value in place.
