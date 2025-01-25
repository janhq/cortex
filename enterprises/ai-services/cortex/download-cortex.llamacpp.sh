#!/bin/bash

VERSION=${1:-latest}

# Get the latest version of the cortex.llamacpp
if [ "$VERSION" = "latest" ]; then
    VERSION=$(curl -s https://api.github.com/repos/janhq/cortex.llamacpp/releases/latest | jq -r '.tag_name' | sed 's/^v//');
fi

# Create the directory to store the cortex.llamacpp
mkdir -p /opt/cortex.llamacpp
cd /opt/cortex.llamacpp

# Download the cortex.llamacpp engines
echo -e "Downloading Cortex Llama version $VERSION"
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx-cuda-11-7.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx-cuda-12-0.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx2-cuda-11-7.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx2-cuda-12-0.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx2.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx512-cuda-11-7.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx512-cuda-12-0.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-avx512.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-noavx-cuda-11-7.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-noavx-cuda-12-0.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-noavx.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cortex.llamacpp-$VERSION-linux-amd64-vulkan.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cuda-11-7-linux-amd64.tar.gz
wget https://github.com/janhq/cortex.llamacpp/releases/download/v$VERSION/cuda-12-0-linux-amd64.tar.gz