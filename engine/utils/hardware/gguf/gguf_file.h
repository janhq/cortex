#pragma once
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <algorithm>
#include <any>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <limits>
#else
#include <sys/mman.h>  // For memory-mapped file
#include <unistd.h>    // For file descriptors
#endif

#include "ggml.h"
#include "utils/logging_utils.h"
#include "utils/string_utils.h"

// #define GGUF_LOG(msg)                                                  \
//   do {                                                                 \
//     std::cout << __FILE__ << "(@" << __LINE__ << "): " << msg << '\n'; \
//   } while (false)

#define GGUF_LOG(msg)
namespace hardware {
#undef min
#undef max

using GGUFMagic = uint32_t;
constexpr const GGUFMagic kGGUFMagicGGML = 0x67676d6c;
constexpr const GGUFMagic kGGUFMagicGGMF = 0x67676d66;
constexpr const GGUFMagic kGGUFMagicGGJT = 0x67676a74;
constexpr const GGUFMagic kGGUFMagicGGUFLe = 0x46554747;  // GGUF
constexpr const GGUFMagic kGGUFMagicGGUFBe = 0x47475546;  // GGUF

using GGUFVersion = uint32_t;
constexpr const GGUFVersion kGGUFVersionV1 = 1;
constexpr const GGUFVersion kGGUFVersionV2 = 2;
constexpr const GGUFVersion kGGUFVersionV3 = 3;

enum GGUFMetadataValueType : uint32_t {
  GGUFMetadataValueTypeUint8 = 0,
  GGUFMetadataValueTypeInt8,
  GGUFMetadataValueTypeUint16,
  GGUFMetadataValueTypeInt16,
  GGUFMetadataValueTypeUint32,
  GGUFMetadataValueTypeInt32,
  GGUFMetadataValueTypeFloat32,
  GGUFMetadataValueTypeBool,
  GGUFMetadataValueTypeString,
  GGUFMetadataValueTypeArray,
  GGUFMetadataValueTypeUint64,
  GGUFMetadataValueTypeInt64,
  GGUFMetadataValueTypeFloat64,
  _GGUFMetadataValueTypeCount  // Unknown
};

struct GGUFMetadataKV {
  // Key is the key of the metadata key-value pair,
  // which is no larger than 64 bytes long.
  std::string key;  // Using std::string for dynamic string handling

  // ValueType is the type of the metadata value.
  GGUFMetadataValueType value_type;  // Enum to represent value types

  // Value is the value of the metadata key-value pair.
  std::any value;
};

struct GGUFMetadataKVArrayValue {
  /* Basic */

  // type is the type of the array item.
  GGUFMetadataValueType type;  // Enum to represent value types

  // Len is the length of the array.
  uint64_t len;  // Using uint64_t for length

  // Array holds all array items.
  std::vector<std::any> arr;
  /* Appendix */

  // start_offset is the offset in bytes of the GGUFMetadataKVArrayValue in the GGUFFile file.
  int64_t start_offset;  // Using int64_t for offset

  // Size is the size of the array in bytes.
  int64_t size;  // Using int64_t for size
};

inline std::string to_string(GGUFMetadataValueType vt, const std::any& v) {
  switch (vt) {
    case GGUFMetadataValueTypeUint8:
      return std::to_string(std::any_cast<uint8_t>(v));
    case GGUFMetadataValueTypeInt8:
      return std::to_string(std::any_cast<int8_t>(v));
    case GGUFMetadataValueTypeUint16:
      return std::to_string(std::any_cast<uint16_t>(v));
    case GGUFMetadataValueTypeInt16:
      return std::to_string(std::any_cast<int16_t>(v));
    case GGUFMetadataValueTypeUint32:
      return std::to_string(std::any_cast<uint32_t>(v));
    case GGUFMetadataValueTypeInt32:
      return std::to_string(std::any_cast<int32_t>(v));
    case GGUFMetadataValueTypeFloat32:
      return std::to_string(std::any_cast<float>(v));
    case GGUFMetadataValueTypeBool:
      return std::to_string(std::any_cast<bool>(v));
    case GGUFMetadataValueTypeString:
      return std::any_cast<std::string>(v);
    case GGUFMetadataValueTypeUint64:
      return std::to_string(std::any_cast<uint64_t>(v));
    case GGUFMetadataValueTypeInt64:
      return std::to_string(std::any_cast<int64_t>(v));
    case GGUFMetadataValueTypeFloat64:
      return std::to_string(std::any_cast<double>(v));
    default:
      break;
  }
  return "array";
}
inline std::string to_string(const GGUFMetadataKVArrayValue& arr_v) {
  std::string res;
  auto num = std::min(size_t(5), arr_v.arr.size());
  for (size_t i = 0; i < num; i++) {
    res += to_string(arr_v.type, arr_v.arr[i]) + " ";
  }
  return res;
}

inline std::string to_string(const GGUFMetadataKV& kv) {
  switch (kv.value_type) {
    case GGUFMetadataValueTypeUint8:
      return std::to_string(std::any_cast<uint8_t>(kv.value));
    case GGUFMetadataValueTypeInt8:
      return std::to_string(std::any_cast<int8_t>(kv.value));
    case GGUFMetadataValueTypeUint16:
      return std::to_string(std::any_cast<uint16_t>(kv.value));
    case GGUFMetadataValueTypeInt16:
      return std::to_string(std::any_cast<int16_t>(kv.value));
    case GGUFMetadataValueTypeUint32:
      return std::to_string(std::any_cast<uint32_t>(kv.value));
    case GGUFMetadataValueTypeInt32:
      return std::to_string(std::any_cast<int32_t>(kv.value));
    case GGUFMetadataValueTypeFloat32:
      return std::to_string(std::any_cast<float>(kv.value));
    case GGUFMetadataValueTypeBool:
      return std::to_string(std::any_cast<bool>(kv.value));
    case GGUFMetadataValueTypeString:
      return std::any_cast<std::string>(kv.value);
    case GGUFMetadataValueTypeUint64:
      return std::to_string(std::any_cast<uint64_t>(kv.value));
    case GGUFMetadataValueTypeInt64:
      return std::to_string(std::any_cast<int64_t>(kv.value));
    case GGUFMetadataValueTypeFloat64:
      return std::to_string(std::any_cast<double>(kv.value));
    case GGUFMetadataValueTypeArray:
      return to_string(std::any_cast<GGUFMetadataKVArrayValue>(kv.value));
    default:
      break;
  }
  return "Invalid type ";
}

struct GGUFTensorInfo {
  /* Basic */
  std::string name;

  // NDimensions is the number of dimensions of the tensor.
  uint32_t n_dimensions;
  // Dimensions is the dimensions of the tensor,
  // the length is NDimensions.
  std::vector<uint64_t> dimensions;
  // type is the type of the tensor.
  GGMLType type;
  // Offset is the offset in bytes of the tensor's data in this file.
  //
  // The offset is relative to tensor data, not to the start of the file.
  uint64_t offset;

  /* Appendix */

  // StartOffset is the offset in bytes of the GGUFTensorInfo in the GGUFFile file.
  //
  // The offset is the start of the file.
  int64_t start_offset;
};

struct GGUFHelper {
  uint8_t* data;
  uint8_t* d_close;
  uint64_t file_size;

  bool OpenAndMMap(const std::string& file_path) {
#ifdef _WIN32
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    HANDLE file_mapping = nullptr;
    file_handle =
        CreateFileA(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
      CTL_INF("Failed to open file: " << file_path);
      return false;
    }
    // Get the file size
    LARGE_INTEGER file_size_struct;
    if (!GetFileSizeEx(file_handle, &file_size_struct)) {
      CloseHandle(file_handle);
      CTL_INF("Failed to get file size: " << file_path);
      return false;
    }
    file_size = static_cast<size_t>(file_size_struct.QuadPart);

    // Create a file mapping object
    file_mapping =
        CreateFileMappingA(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (file_mapping == nullptr) {
      CloseHandle(file_handle);
      CTL_INF("Failed to create file mapping: " << file_path);
      return false;
    }

    // Map the file into memory
    data = static_cast<uint8_t*>(
        MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, file_size));
    if (data == nullptr) {
      CloseHandle(file_mapping);
      CloseHandle(file_handle);
      CTL_INF("Failed to map file:: " << file_path);
      return false;
    }

    // Close the file handle, as it is no longer needed after mapping
    CloseHandle(file_handle);
    d_close = data;
#else
    file_size = std::filesystem::file_size(file_path);

    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd == -1) {
      CTL_INF("Failed to open file: " << file_path << ", error: " << errno);
      return false;
    }
    // Memory-map the file
    data = static_cast<uint8_t*>(
        mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
      CTL_INF("Error mapping file");
      close(fd);
      return false;
    }

    close(fd);
    d_close = data;
#endif
    return true;
  }

  ~GGUFHelper() { Close(); }

  void Close() {
#ifdef _WIN32
    if (d_close != nullptr) {
      UnmapViewOfFile(d_close);
      d_close = nullptr;
    }
#else
    if (d_close != nullptr && d_close != MAP_FAILED) {
      munmap(d_close, file_size);
      d_close = nullptr;
    }
#endif
  }

  template <typename T>
  T Read() {
    static_assert(std::is_floating_point<T>::value ||
                  std::is_integral<T>::value || std::is_same<T, bool>::value);
    T res = *reinterpret_cast<const T*>(data);
    data += sizeof(T);
    return res;
  }

  std::string ReadString() {
    auto l = Read<uint64_t>();
    std::string res(reinterpret_cast<const char*>(data), l);
    auto r = res;
    data += l;
    return r;
  }

  GGUFMetadataKVArrayValue ReadArray() {
    GGUFMetadataKVArrayValue v;
    v.start_offset = (data - d_close);
    v.type = static_cast<GGUFMetadataValueType>(Read<uint32_t>());
    auto arr_length = Read<uint64_t>();
    for (uint64_t i = 0; i < arr_length; ++i) {
      switch (v.type) {
        case GGUFMetadataValueTypeUint8:
          v.arr.push_back(Read<uint8_t>());
          break;
        case GGUFMetadataValueTypeInt8:
          v.arr.push_back(Read<int8_t>());
          break;
        case GGUFMetadataValueTypeUint16:
          v.arr.push_back(Read<uint16_t>());
          break;
        case GGUFMetadataValueTypeInt16:
          v.arr.push_back(Read<uint16_t>());
          break;
        case GGUFMetadataValueTypeUint32:
          v.arr.push_back(Read<uint32_t>());
          break;
        case GGUFMetadataValueTypeInt32:
          v.arr.push_back(Read<int32_t>());
          break;
        case GGUFMetadataValueTypeFloat32:
          v.arr.push_back(Read<float>());
          break;
        case GGUFMetadataValueTypeBool:
          v.arr.push_back(Read<bool>());
          break;
        case GGUFMetadataValueTypeString:
          v.arr.push_back(ReadString());
          break;
        case GGUFMetadataValueTypeUint64:
          v.arr.push_back(Read<uint64_t>());
          break;
        case GGUFMetadataValueTypeInt64:
          v.arr.push_back(Read<int64_t>());
          break;
        case GGUFMetadataValueTypeFloat64:
          v.arr.push_back(Read<double>());
          break;
        default:
          std::cout << "Invalid type: " << std::to_string(v.type);
      }
    }
    v.size = data - v.start_offset - d_close - 4 - 8;
    return v;
  }

  std::any ReadValue(GGUFMetadataValueType vt) {
    switch (vt) {
      case GGUFMetadataValueTypeUint8:
        return Read<uint8_t>();
      case GGUFMetadataValueTypeInt8:
        return Read<int8_t>();
      case GGUFMetadataValueTypeUint16:
        return Read<uint16_t>();
      case GGUFMetadataValueTypeInt16:
        return Read<uint16_t>();
      case GGUFMetadataValueTypeUint32:
        return Read<uint32_t>();
      case GGUFMetadataValueTypeInt32:
        return Read<int32_t>();
      case GGUFMetadataValueTypeFloat32:
        return Read<float>();
      case GGUFMetadataValueTypeBool:
        return Read<bool>();
      case GGUFMetadataValueTypeString:
        return ReadString();
      case GGUFMetadataValueTypeArray:
        return ReadArray();
      case GGUFMetadataValueTypeUint64:
        return Read<uint64_t>();
      case GGUFMetadataValueTypeInt64:
        return Read<int64_t>();
      case GGUFMetadataValueTypeFloat64:
        return Read<double>();
      default:
        std::cout << "Invalid type: " << vt;
        return {};
    }
  }

  GGUFMetadataKV ReadMetadataKV() {
    GGUFMetadataKV kv;
    kv.key = ReadString();
    auto vt = Read<uint32_t>();
    kv.value_type = GGUFMetadataValueType(vt);
    kv.value = ReadValue(kv.value_type);
    return kv;
  }

  std::shared_ptr<GGUFTensorInfo> ReadTensorInfo() {
    auto ti = std::make_shared<GGUFTensorInfo>();
    ti->start_offset = data - d_close;
    ti->name = ReadString();
    ti->n_dimensions = Read<uint32_t>();
    ti->dimensions.resize(ti->n_dimensions);
    for (size_t i = 0; i < ti->n_dimensions; i++) {
      ti->dimensions[i] = Read<uint64_t>();
    }
    auto v = Read<uint32_t>();
    ti->type = GGMLType(v);
    ti->offset = Read<uint64_t>();
    return ti;
  }
};

constexpr const auto ErrGGUFFileInvalidFormat = "invalid GGUF format";

struct GGUFHeader {
  // Magic is a magic number that announces that this is a GGUF file.
  GGUFMagic magic;
  // Version is a version of the GGUF file format.
  GGUFVersion version;
  // TensorCount is the number of tensors in the file.
  uint64_t tensor_count;
  // MetadataKVCount is the number of key-value pairs in the metadata.
  uint64_t metadata_kv_count;
  // MetadataKV are the key-value pairs in the metadata,
  std::vector<GGUFMetadataKV> metadata_kv;

  std::pair<GGUFMetadataKV, bool> Get(const std::string& name) {
    for (auto const& kv : metadata_kv) {
      if (kv.key == name) {
        return std::pair(kv, true);
      }
    }
    return std::pair(GGUFMetadataKV{}, false);
  }
};

struct GGUFFile {
  // header is the header of the GGUF file.
  GGUFHeader header;
  // tensor_infos are the tensor infos of the GGUF file,
  // the size of TensorInfos is equal to `Header.TensorCount`.
  std::vector<std::shared_ptr<GGUFTensorInfo>> tensor_infos;

  // padding is the padding size of the GGUF file,
  // which is used to split Header and TensorInfos from tensor data.
  int64_t padding;
  // split_paddings holds the padding size slice of the GGUF file splits,
  // each item represents splitting Header and TensorInfos from tensor data.
  //
  // The length of split_paddings is the number of split files.
  std::vector<int64_t> split_paddings;
  // tensor_data_start_offset is the offset in bytes of the tensor data in this file.
  //
  // The offset is the start of the file.
  int64_t tensor_data_start_offset;
  // split_tensor_data_start_offsets holds the offset slice in bytes of the tensor data of the GGUF file splits,
  // each item represents the offset of the tensor data in the split file.
  //
  // The length of split_tensor_data_start_offsets is the number of split files.
  std::vector<int64_t> split_tensor_data_start_offsets;

  /* Appendix */

  // size is the size of the GGUF file,
  // if the file is split, the size is the sum of all split files.
  uint64_t size;
  // split_sizes holds the size slice of the GGUF file splits,
  // each item represents the size of the split file.
  //
  // The length of split_sizes is the number of split files.
  std::vector<uint64_t> split_sizes;
  // model_size is the size of the model when loading.
  uint64_t model_size;
  // split_model_sizes holds the size slice of the model,
  // each item represents a size when loading of the split file.
  //
  // The length of split_model_sizes is the number of split files.
  std::vector<uint64_t> split_model_sizes;

  // model_parameters is the number of the model parameters.
  uint64_t model_parameters;
  // model_bits_per_weight is the bits per weight of the model,
  // which describes how many bits are used to store a weight,
  // higher is better.
  double model_bits_per_weight;
};

inline std::optional<GGUFFile> ParseGgufFile(const std::string& path) {
  GGUFFile gf;
  GGUFHelper h;
  if (!h.OpenAndMMap(path)) {
    return std::nullopt;
  }

  GGUFMagic magic = h.Read<GGUFMagic>();
  // GGUF_LOG("magic: " << magic);
  gf.header.magic = magic;
  GGUFVersion version = h.Read<GGUFVersion>();
  auto tensor_count = h.Read<uint64_t>();
  // GGUF_LOG("tensor_count: " << tensor_count);
  gf.header.tensor_count += tensor_count;

  auto metadata_kv_count = h.Read<uint64_t>();
  gf.header.metadata_kv_count += metadata_kv_count;
  // GGUF_LOG("metadata_kv_count: " << metadata_kv_count);

  // metadata kv
  {
    std::vector<GGUFMetadataKV> kvs;
    kvs.resize(metadata_kv_count);
    for (size_t i = 0; i < metadata_kv_count; i++) {
      kvs[i] = h.ReadMetadataKV();
      GGUF_LOG("i: " << i << " " << kvs[i].value_type << " " << kvs[i].key
                     << ": " << to_string(kvs[i]));
    }
    for (auto const& kv : kvs) {
      if (kv.key == "split.no") {
        gf.header.metadata_kv_count--;
        continue;
      }
      gf.header.metadata_kv.push_back(kv);
    }
  }

  {
    std::vector<std::shared_ptr<GGUFTensorInfo>> tis;
    tis.resize(tensor_count);
    for (size_t i = 0; i < tensor_count; i++) {
      tis[i] = h.ReadTensorInfo();
      // auto tto_string = [](const std::vector<size_t>& ds) -> std::string {
      //   std::string res = "[";
      //   for (auto d : ds)
      //     res += std::to_string(d) + " ";
      //   return res + "]";
      // };
      // auto ds = tto_string(tis[i]->dimensions);
      // GGUF_LOG("i: " << i << " name: " << tis[i]->name
      //                << " type: " << to_string(tis[i]->type) << " dimensions: "
      //                << std::to_string(tis[i]->n_dimensions) << " " << ds);
    }
    gf.tensor_infos = tis;
  }
  return gf;
}
}  // namespace hardware