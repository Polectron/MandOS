#pragma once

#include <device_registry.h>

// Persists the registry in flash (LittleFS) so devices survive reboots.
// Restored devices start offline until they answer a ping.
namespace core_store {

// Loads saved devices into `registry`. Returns the number restored.
size_t load(DeviceRegistry &registry);

bool save(DeviceRegistry &registry);

}
