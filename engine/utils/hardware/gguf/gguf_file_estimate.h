#pragma once
#include <algorithm>
#include <regex>
#include "gguf_file.h"
#include "json/json.h"

namespace hardware {
inline uint64_t BytesToMiB(uint64_t b) {
  return (double)b / 1024 / 1024;
};
struct RunConfig {
  int ngl;
  int ctx_len;
  int n_batch;
  int n_ubatch;
  std::string kv_cache_type;
  int64_t free_vram_MiB;
};

struct CpuMode {
  int64_t ram_MiB;
};

struct GpuMode {
  int64_t ram_MiB;
  int64_t vram_MiB;
  int ngl;
  int ctx_len;
  int recommend_ngl;
};

struct Estimation {
  CpuMode cpu_mode;
  GpuMode gpu_mode;
};

inline Json::Value ToJson(const Estimation& es) {
  Json::Value res;
  Json::Value cpu;
  cpu["ram"] = es.cpu_mode.ram_MiB;
  Json::Value gpus(Json::arrayValue);
  Json::Value gpu;
  gpu["ram"] = es.gpu_mode.ram_MiB;
  gpu["vram"] = es.gpu_mode.vram_MiB;
  gpu["ngl"] = es.gpu_mode.ngl;
  gpu["context_length"] = es.gpu_mode.ctx_len;
  gpu["recommend_ngl"] = es.gpu_mode.recommend_ngl;
  gpus.append(gpu);
  res["cpu_mode"] = cpu;
  res["gpu_mode"] = gpus;
  return res;
}

inline float GetQuantBit(const std::string& kv_cache_t) {
  if (kv_cache_t == "f16") {
    return 16.0;
  } else if (kv_cache_t == "q8_0") {
    return 8.0;
  } else if (kv_cache_t == "q4_0") {
    return 4.5;
  }
  return 16.0;
}

inline std::optional<Estimation> EstimateLLaMACppRun(
    const std::string& file_path, const RunConfig& rc) {
  // token_embeddings_size = n_vocab * embedding_length * 2 * quant_bit/16 bytes
  //RAM = token_embeddings_size + ((total_ngl-ngl) >=1 ? Output_layer_size +  (total_ngl - ngl - 1 ) / (total_ngl-1) * (total_file_size - token_embeddings_size - Output_layer_size) : 0  )  (bytes)

  // VRAM = total_file_size - RAM (bytes)
  auto gf = ParseGgufFile(file_path);
  if (!gf)
    return std::nullopt;
  Estimation res;
  int32_t embedding_length = 0;
  int64_t n_vocab = 0;
  int32_t num_block = 0;
  int32_t total_ngl = 0;
  auto file_size = std::filesystem::file_size(file_path);
  for (auto const& kv : (*gf).header.metadata_kv) {
    if (kv.key.find("embedding_length") != std::string::npos) {
      embedding_length = std::any_cast<uint32_t>(kv.value);
    } else if (kv.key == "tokenizer.ggml.tokens") {
      n_vocab = std::any_cast<GGUFMetadataKVArrayValue>(kv.value).arr.size();
    } else if (kv.key.find("block_count") != std::string::npos) {
      num_block = std::any_cast<uint32_t>(kv.value);
      total_ngl = num_block + 1;
    }
  }

  // std::cout << n_vocab << std::endl;

  // token_embeddings_size = n_vocab * embedding_length * 2 * quant_bit_in/16 bytes
  int32_t quant_bit_in = 0;
  int32_t quant_bit_out = 0;

  for (auto const& ti : (*gf).tensor_infos) {
    if (ti->name == "output.weight") {
      quant_bit_out = GetQuantBit(ti->type);
      // std::cout << ti->type << std::endl;
    } else if (ti->name == "token_embd.weight") {
      quant_bit_in = GetQuantBit(ti->type);
      // std::cout << ti->type << std::endl;
    }
  }
  // output.weight
  // token_embd.weight
  // std::cout << "embedding_length: " << embedding_length << std::endl;
  // std::cout << "n_vocab: " << n_vocab << std::endl;
  // std::cout << "file_size: " << file_size << std::endl;
  // Model weight
  int64_t token_embeddings_size =
      n_vocab * embedding_length * 2 * quant_bit_in / 16;
  int64_t output_layer_size =
      n_vocab * embedding_length * 2 * quant_bit_out / 16;
  // RAM = token_embeddings_size + ((total_ngl-ngl) >=1 ? output_layer_size +  (total_ngl - ngl - 1 ) / (total_ngl-1) * (total_file_size - token_embeddings_size - output_layer_size) : 0  )  (bytes)
  int64_t offload = 0;
  if (total_ngl >= rc.ngl + 1) {
    offload = output_layer_size +
              (double)(total_ngl - rc.ngl - 1) / (total_ngl - 1) *
                  (file_size - token_embeddings_size - output_layer_size);
  }

  int64_t ram_usage = token_embeddings_size + offload;
  int64_t vram_usage = file_size - ram_usage;
  // std::cout << "token_embeddings_size: " << BytesToMiB(token_embeddings_size)
  //           << std::endl;
  // std::cout << "output_layer_size: " << BytesToMiB(output_layer_size)
  //           << std::endl;
  // std::cout << "ram_usage: " << BytesToMiB(ram_usage) << std::endl;
  // std::cout << "vram_usage: " << BytesToMiB(vram_usage) << std::endl;

  // KV cache
  // kv_cache_size = ctx_len/8192 * hidden_dim/4096 * quant_bit/16 * num_block/33 * 1 (GB)
  auto hidden_dim = embedding_length;
  int kv_quant_bit =
      GetQuantBit(rc.kv_cache_type);  // f16, 8 bits for q8_0, 4.5 bits for q4_0
  int64_t kv_cache_size = (double)(1024 * 1024 * 1024) * rc.ctx_len / 8192 *
                          hidden_dim / 4096 * kv_quant_bit / 16 * num_block /
                          33;  //(bytes)

  // std::cout << "kv_cache_size: " << BytesToMiB(kv_cache_size) << std::endl;

  // VRAM = (min(n_batch, n_ubatch))/ 512 * 266 (MiB)
  int64_t preprocessing_buffer_size =
      (double)std::min(rc.n_batch, rc.n_ubatch) / 512 * 266 * 1024 * 1024 *
      n_vocab / 128256 /*llama3 n_vocab*/;  //(bytes)
  if (total_ngl != rc.ngl) {
    preprocessing_buffer_size += output_layer_size;
  }
  // std::cout << "preprocessing_buffer_size: "
  //           << BytesToMiB(preprocessing_buffer_size) << std::endl;

  // CPU mode
  {
    // Model weight
    int64_t model_weight = file_size;
    // KV cache
    // Buffer
    res.cpu_mode.ram_MiB =
        BytesToMiB(model_weight + kv_cache_size + preprocessing_buffer_size);
  }
  // GPU mode
  {
    res.gpu_mode.ctx_len = rc.ctx_len;
    res.gpu_mode.ngl = rc.ngl;
    res.gpu_mode.ram_MiB = BytesToMiB(ram_usage);
    // We also need to reserve extra 100 MiB -200 MiB of Ram for some small buffers during processing
    constexpr const int64_t kDeltaVramMiB = 200;
    res.gpu_mode.vram_MiB =
        kDeltaVramMiB +
        BytesToMiB(vram_usage + kv_cache_size + preprocessing_buffer_size);
    if (rc.free_vram_MiB > res.gpu_mode.vram_MiB) {
      res.gpu_mode.recommend_ngl = total_ngl;
    } else {
      res.gpu_mode.recommend_ngl =
          (double)rc.free_vram_MiB / res.gpu_mode.vram_MiB * rc.ngl;
    }
#if defined(__APPLE__) && defined(__MACH__)
    res.cpu_mode.ram_MiB = res.gpu_mode.vram_MiB + res.gpu_mode.ram_MiB;
#endif
  }
  return res;
}
}  // namespace hardware