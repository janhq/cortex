# cortex.llamacpp release version
set(VERSION 0.1.25)

set(ENGINE_VERSION v${VERSION})
add_compile_definitions(CORTEX_LLAMACPP_VERSION="${VERSION}")

# MESSAGE("ENGINE_VERSION=" ${ENGINE_VERSION})

# Download library based on instructions 
if(UNIX AND NOT APPLE) 
  if(CUDA_12_0)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-linux-amd64-noavx-cuda-12-0.tar.gz)
  elseif(CUDA_11_7)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-linux-amd64-noavx-cuda-11-7.tar.gz)
  elseif(LLAMA_VULKAN)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-linux-amd64-vulkan.tar.gz)
    set(ENGINE_VERSION v${VERSION})
  else()
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-linux-amd64-noavx.tar.gz)
  endif()
elseif(UNIX)
  if(MAC_ARM64) 
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-mac-arm64.tar.gz)
  else()
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-mac-amd64.tar.gz)
  endif()
else()
  if(CUDA_12_0)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-windows-amd64-noavx-cuda-12-0.tar.gz)
  elseif(CUDA_11_7)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-windows-amd64-noavx-cuda-11-7.tar.gz)
  elseif(LLAMA_VULKAN)
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-windows-amd64-vulkan.tar.gz)
    set(ENGINE_VERSION v${VERSION})
  else()
    set(LIBRARY_NAME cortex.llamacpp-${VERSION}-windows-amd64-noavx.tar.gz)
  endif()
endif()


set(LIBLLAMA_ENGINE_URL https://github.com/janhq/cortex.llamacpp/releases/download/${ENGINE_VERSION}/${LIBRARY_NAME})
# MESSAGE("LIBLLAMA_ENGINE_URL="${LIBLLAMA_ENGINE_URL})
# MESSAGE("LIBARRY_NAME=" ${LIBRARY_NAME})

set(LIBLLAMA_ENGINE_PATH ${CMAKE_BINARY_DIR}/engines/${LIBRARY_NAME})

# MESSAGE("CMAKE_BINARY_DIR = " ${CMAKE_BINARY_DIR})

file(DOWNLOAD ${LIBLLAMA_ENGINE_URL} ${LIBLLAMA_ENGINE_PATH} STATUS LIBLLAMA_ENGINE_DOWNLOAD_STATUS)
list(GET LIBLLAMA_ENGINE_DOWNLOAD_STATUS 0 LIBLLAMA_ENGINE_DOWNLOAD_STATUS_NO)
# MESSAGE("file = " ${CMAKE_BINARY_DIR}/engines/${LIBRARY_NAME})

if(LIBLLAMA_ENGINE_DOWNLOAD_STATUS_NO)
    message(STATUS "Pre-built library not downloaded. (${LIBLLAMA_ENGINE_DOWNLOAD_STATUS})")
else()
    message(STATUS "Linking downloaded pre-built library.")
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_BINARY_DIR}/engines/${LIBRARY_NAME} DESTINATION ${CMAKE_BINARY_DIR}/engines/)
endif()