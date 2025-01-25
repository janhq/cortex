FROM docker.io/pytorch/pytorch:2.2.0-cuda12.1-cudnn8-runtime

# Set the MKL_SERVICE_FORCE_INTEL environment variable
ENV MKL_SERVICE_FORCE_INTEL=1

# Install dependencies
RUN apt-get -qq update; \
    apt-get install -qqy --no-install-recommends \
        gnupg2 wget ca-certificates apt-transport-https \
        autoconf automake cmake dpkg-dev file make patch libc6-dev git

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    sudo \
    unzip \
    curl \
    wget \
    git \
    git-lfs \
    jq \
  && rm -rf /var/lib/apt/lists/*
RUN apt-get -y update \
    && apt-get install -y software-properties-common \
    && apt-get -y update \
    && add-apt-repository universe

WORKDIR /app
COPY ./requirements.txt /app
RUN pip3 install -r requirements.txt
