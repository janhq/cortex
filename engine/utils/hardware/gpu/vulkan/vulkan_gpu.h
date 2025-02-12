#pragma once

#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/hardware_common.h"
#include "utils/file_manager_utils.h"
#include "utils/logging_utils.h"
#include "utils/result.hpp"
#include "utils/widechar_conv.h"
#include "vulkan.h"

#if defined(_WIN32)
#include "../adl/adl_sdk.h"
#include "../adl/amd_ags.h"
#else
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#endif

namespace cortex::hw {
#if defined(_WIN32)
// Definitions of the used function pointers. Add more if you use other ADL APIs
typedef int (*ADL_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (*ADL_MAIN_CONTROL_DESTROY)();
typedef int (*ADL_ADAPTER_NUMBEROFADAPTERS_GET)(int*);
typedef int (*ADL_ADAPTER_ADAPTERINFO_GET)(LPAdapterInfo, int);
typedef int (*ADL_ADAPTER_ACTIVE_GET)(int, int*);
typedef int (*ADL_OVERDRIVE_CAPS)(int iAdapterIndex, int* iSupported,
                                  int* iEnabled, int* iVersion);
typedef int (*ADL_GET_DEDICATED_VRAM_USAGE)(ADL_CONTEXT_HANDLE context,
                                            int iAdapterIndex,
                                            int* vram_usage_in_MB);

// Memory allocation function
inline void* __stdcall ADL_Main_Memory_Alloc(int iSize) {
  void* lpBuffer = malloc(iSize);
  return lpBuffer;
}

// Optional Memory de-allocation function
inline void __stdcall ADL_Main_Memory_Free(void** lpBuffer) {
  if (nullptr != *lpBuffer) {
    free(*lpBuffer);
    *lpBuffer = nullptr;
  }
}

inline cpp::result<std::unordered_map<std::string, int>, std::string>
GetGpuUsage() {
  HINSTANCE hDLL;  // Handle to DLL

  LPAdapterInfo lpAdapterInfo = nullptr;
  int i;
  int num_adapters = 0;

  hDLL = LoadLibrary(L"atiadlxx.dll");
  if (hDLL == nullptr)
    // A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
    // Try to load the 32 bit library (atiadlxy.dll) instead
    hDLL = LoadLibrary(L"atiadlxy.dll");

  if (nullptr == hDLL) {
    return cpp::fail("ADL library not found!");
  }

  auto ADL_Main_Control_Create =
      (ADL_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL_Main_Control_Create");
  auto ADL_Main_Control_Destroy = (ADL_MAIN_CONTROL_DESTROY)GetProcAddress(
      hDLL, "ADL_Main_Control_Destroy");
  auto ADL_Adapter_NumberOfAdapters_Get =
      (ADL_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(
          hDLL, "ADL_Adapter_NumberOfAdapters_Get");
  auto ADL_Adapter_AdapterInfo_Get =
      (ADL_ADAPTER_ADAPTERINFO_GET)GetProcAddress(
          hDLL, "ADL_Adapter_AdapterInfo_Get");
  auto ADL_Adapter_Active_Get =
      (ADL_ADAPTER_ACTIVE_GET)GetProcAddress(hDLL, "ADL_Adapter_Active_Get");

  auto ADL_Get_Dedicated_Vram_Usage =
      (ADL_GET_DEDICATED_VRAM_USAGE)GetProcAddress(
          hDLL, "ADL2_Adapter_DedicatedVRAMUsage_Get");

  if (nullptr == ADL_Main_Control_Create ||
      nullptr == ADL_Main_Control_Destroy ||
      nullptr == ADL_Adapter_NumberOfAdapters_Get ||
      nullptr == ADL_Adapter_AdapterInfo_Get ||
      nullptr == ADL_Adapter_Active_Get ||
      nullptr == ADL_Get_Dedicated_Vram_Usage) {
    return cpp::fail("ADL's API is missing!");
  }

  // Initialize ADL. The second parameter is 1, which means:
  // retrieve adapter information only for adapters that are physically present and enabled in the system
  if (ADL_OK != ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1)) {
    return cpp::fail("ADL Initialization Error!");
  }

  // Obtain the number of adapters for the system
  if (ADL_OK != ADL_Adapter_NumberOfAdapters_Get(&num_adapters)) {
    return cpp::fail("Cannot get the number of adapters!");
  }
  // std::cout << "num_adapters: " << num_adapters << std::endl;

  if (0 < num_adapters) {
    lpAdapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * num_adapters);
    memset(lpAdapterInfo, '\0', sizeof(AdapterInfo) * num_adapters);

    // Get the AdapterInfo structure for all adapters in the system
    ADL_Adapter_AdapterInfo_Get(lpAdapterInfo,
                                sizeof(AdapterInfo) * num_adapters);
  }

  // Looking for first present and active adapter in the system
  int adapter_id = -1;
  std::unordered_map<std::string, int> vram_usages;
  for (i = 0; i < num_adapters; i++) {
    int adapter_active = 0;
    AdapterInfo adapter_info = lpAdapterInfo[i];
    ADL_Adapter_Active_Get(adapter_info.iAdapterIndex, &adapter_active);

    if (ADL_Get_Dedicated_Vram_Usage) {
      int vram_usage_in_MB = 0;
      ADL_Get_Dedicated_Vram_Usage(nullptr, i, &vram_usage_in_MB);
      vram_usages[adapter_info.strAdapterName] = vram_usage_in_MB;
    }
  }

  ADL_Main_Control_Destroy();
  return vram_usages;
}

#else

struct AmdGpuUsage {
  int64_t total_vram_MiB;
  int64_t used_vram_MiB;
};

inline cpp::result<std::unordered_map<int, AmdGpuUsage>, std::string>
GetGpuUsage() {
  // list all devices
  std::unordered_map<int, AmdGpuUsage> res;
  std::string path = "/sys/class/drm/";
  try {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      auto const& es = entry.path().stem().string();
      if (entry.is_directory() && es.find("card") != std::string::npos &&
          es.find("-") == std::string::npos) {
        // std::cout << entry.path() << std::endl;
        std::filesystem::path gpu_device_path = entry.path() / "device";
        std::filesystem::path vendor_path = gpu_device_path / "vendor";
        std::ifstream vendor_file(vendor_path.string());
        if (vendor_file.is_open()) {
          std::string vendor_str;
          std::getline(vendor_file, vendor_str);
          vendor_file.close();

          // std::cout << "Vendor: " << vendor_str << std::endl;
          if (vendor_str == "0x1002") {
            std::filesystem::path vram_total_path =
                gpu_device_path / "mem_info_vram_total";
            std::filesystem::path vram_used_path =
                gpu_device_path / "mem_info_vram_used";
            std::filesystem::path device_id_path = gpu_device_path / "device";
            auto get_vram = [](const std::filesystem::path& p,
                               int base) -> int64_t {
              std::ifstream f(p.string());
              if (f.is_open()) {
                std::string f_str;
                std::getline(f, f_str);
                f.close();
                return std::stoll(f_str, nullptr, base);
              } else {
                std::cerr << "Error: Unable to open " << p.string()
                          << std::endl;
                return -1;
              }
            };
            auto vram_total = get_vram(vram_total_path, 10) / 1024 / 1024;
            auto vram_usage = get_vram(vram_used_path, 10) / 1024 / 1024;
            auto device_id = get_vram(device_id_path, 16);
            res[device_id] = AmdGpuUsage{.total_vram_MiB = vram_total,
                                         .used_vram_MiB = vram_usage};
          }
        } else {
          return cpp::fail("Error: Unable to open " + vendor_path.string());
        }
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    return cpp::fail("Error: " + std::string(ex.what()));
  }

  return res;
}

#endif

// Function pointer typedefs
typedef VkResult(VKAPI_PTR* PFN_vkCreateInstance)(const VkInstanceCreateInfo*,
                                                  const VkAllocationCallbacks*,
                                                  VkInstance*);
typedef VkResult(VKAPI_PTR* PFN_vkEnumeratePhysicalDevices)(VkInstance,
                                                            uint32_t*,
                                                            VkPhysicalDevice*);
typedef void(VKAPI_PTR* PFN_vkGetPhysicalDeviceProperties)(
    VkPhysicalDevice, VkPhysicalDeviceProperties*);
typedef void(VKAPI_PTR* PFN_vkDestroyInstance)(VkInstance,
                                               const VkAllocationCallbacks*);

typedef void(VKAPI_PTR* PFN_vkGetPhysicalDeviceMemoryProperties)(
    VkPhysicalDevice physical_device,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties);

typedef void(VKAPI_PTR* PFN_vkGetPhysicalDeviceProperties2)(
    VkPhysicalDevice physical_device, VkPhysicalDeviceProperties2* pProperties);

typedef VkResult(VKAPI_PTR* PFN_vkEnumerateInstanceExtensionProperties)(
    const char* pLayerName, uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties);

#if defined(__linux__)
inline void* GetProcAddress(void* pLibrary, const char* name) {
  return dlsym(pLibrary, name);
}

inline int FreeLibrary(void* pLibrary) {
  return dlclose(pLibrary);
}
#endif

inline cpp::result<std::vector<cortex::hw::GPU>, std::string> GetGpuInfoList() {
  namespace fmu = file_manager_utils;
  auto get_vulkan_path = [](const std::string& lib_vulkan)
      -> cpp::result<std::filesystem::path, std::string> {
    if (std::filesystem::exists(fmu::GetExecutableFolderContainerPath() /
                                lib_vulkan)) {
      return fmu::GetExecutableFolderContainerPath() / lib_vulkan;
      // fallback to deps path
    } else if (std::filesystem::exists(fmu::GetCortexDataPath() / "deps" /
                                       lib_vulkan)) {
      return fmu::GetCortexDataPath() / "deps" / lib_vulkan;
    } else {
      CTL_WRN("Could not found " << lib_vulkan);
      return cpp::fail("Could not found " + lib_vulkan);
    }
  };
// Load the Vulkan library
#if defined(__APPLE__) && defined(__MACH__)
  return std::vector<cortex::hw::GPU>{};
#elif defined(__linux__)
  auto vulkan_path = get_vulkan_path("libvulkan.so");
  if (vulkan_path.has_error()) {
    return cpp::fail(vulkan_path.error());
  }
  void* vulkan_library =
      dlopen(vulkan_path.value().string().c_str(), RTLD_LAZY | RTLD_GLOBAL);
#else
  auto vulkan_path = get_vulkan_path("vulkan-1.dll");
  if (vulkan_path.has_error()) {
    return cpp::fail(vulkan_path.error());
  }
  HMODULE vulkan_library = LoadLibraryW(vulkan_path.value().wstring().c_str());
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
  if (!vulkan_library) {
    std::cerr << "Failed to load the Vulkan library." << std::endl;
    return cpp::fail("Failed to load the Vulkan library.");
  }

  // Get the function pointers for other Vulkan functions
  auto vkEnumerateInstanceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          GetProcAddress(vulkan_library,
                         "vkEnumerateInstanceExtensionProperties"));
  auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      GetProcAddress(vulkan_library, "vkCreateInstance"));
  auto vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          GetProcAddress(vulkan_library, "vkEnumeratePhysicalDevices"));
  auto vkGetPhysicalDeviceProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          GetProcAddress(vulkan_library, "vkGetPhysicalDeviceProperties"));
  auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      GetProcAddress(vulkan_library, "vkDestroyInstance"));
  auto vkGetPhysicalDeviceMemoryProperties =
      (PFN_vkGetPhysicalDeviceMemoryProperties)GetProcAddress(
          vulkan_library, "vkGetPhysicalDeviceMemoryProperties");

  auto vkGetPhysicalDeviceProperties2 =
      (PFN_vkGetPhysicalDeviceProperties2)GetProcAddress(
          vulkan_library, "vkGetPhysicalDeviceProperties2");

  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                         available_extensions.data());

  // Create a Vulkan instance
  VkInstanceCreateInfo instance_create_info = {};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  // If the extension is available, enable it
  std::vector<const char*> enabled_extensions;

  for (const auto& extension : available_extensions) {
    enabled_extensions.push_back(extension.extensionName);
  }

  instance_create_info.enabledExtensionCount =
      static_cast<uint32_t>(available_extensions.size());
  instance_create_info.ppEnabledExtensionNames = enabled_extensions.data();

  VkInstance instance;
  if (vkCreateInstance == nullptr || vkEnumeratePhysicalDevices == nullptr ||
      vkGetPhysicalDeviceProperties == nullptr ||
      vkDestroyInstance == nullptr ||
      vkGetPhysicalDeviceMemoryProperties == nullptr ||
      vkGetPhysicalDeviceProperties2 == nullptr) {
    return cpp::fail("vulkan API is missing!");
  }

  VkResult result = vkCreateInstance(&instance_create_info, nullptr, &instance);
  if (result != VK_SUCCESS) {
    FreeLibrary(vulkan_library);
    return cpp::fail("Failed to create a Vulkan instance.");
  }

  // Get the physical devices
  uint32_t physical_device_count = 0;
  result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
  if (result != VK_SUCCESS) {
    vkDestroyInstance(instance, nullptr);
    FreeLibrary(vulkan_library);
    return cpp::fail("Failed to enumerate physical devices.");
  }
  std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
  vkEnumeratePhysicalDevices(instance, &physical_device_count,
                             physical_devices.data());

  auto uuid_to_string = [](const uint8_t* device_uuid) -> std::string {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint32_t i = 0; i < VK_UUID_SIZE; ++i) {
      if (i == 4 || i == 6 || i == 8 || i == 10) {
        ss << '-';
      }
      ss << std::setw(2) << static_cast<int>(device_uuid[i]);
    }
    return ss.str();
  };

  std::vector<cortex::hw::GPU> gpus;
#if defined(__linux__)
  auto gpus_usages =
      GetGpuUsage().value_or(std::unordered_map<int, AmdGpuUsage>{});
#elif defined(_WIN32)
  auto gpus_usages =
      GetGpuUsage().value_or(std::unordered_map<std::string, int>{});
#endif

  // Get the device properties
  size_t id = 0;
  for (const auto& physical_device : physical_devices) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    VkPhysicalDeviceIDProperties device_id_properties = {};
    VkPhysicalDeviceProperties2 device_properties2 = {};
    device_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    device_properties2.pNext = &device_id_properties;
    device_id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    vkGetPhysicalDeviceProperties2(physical_device, &device_properties2);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    int gpu_avail_MiB = 0;
    for (uint32_t i = 0; i < memory_properties.memoryHeapCount; ++i) {
      if (memory_properties.memoryHeaps[i].flags &
          VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        gpu_avail_MiB +=
            memory_properties.memoryHeaps[i].size / (1024ull * 1024ull);
      }
    }

    int64_t total_vram_MiB = 0;
    int64_t used_vram_MiB = 0;

#if defined(__linux__)
    total_vram_MiB = gpus_usages[device_properties.deviceID].total_vram_MiB;
    used_vram_MiB = gpus_usages[device_properties.deviceID].used_vram_MiB;
#elif defined(_WIN32)
    total_vram_MiB = gpu_avail_MiB;
    used_vram_MiB = gpus_usages[device_properties.deviceName];

#endif
    int free_vram_MiB =
        total_vram_MiB > used_vram_MiB ? total_vram_MiB - used_vram_MiB : 0;
    gpus.emplace_back(cortex::hw::GPU{
        .id = std::to_string(id),
        .device_id = device_properties.deviceID,
        .name = device_properties.deviceName,
        .version = std::to_string(device_properties.driverVersion),
        .add_info = cortex::hw::AmdAddInfo{},
        .free_vram = free_vram_MiB,
        .total_vram = total_vram_MiB,
        .uuid = uuid_to_string(device_id_properties.deviceUUID)});
    id++;
  }

  // Clean up
  vkDestroyInstance(instance, nullptr);
  FreeLibrary(vulkan_library);
  return gpus;
#endif
}
}  // namespace cortex::hw