# syntax=docker/dockerfile:1.4
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Etc/UTC

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        llvm \
        ninja-build \
        git \
        python3 \
        python3-pip \
        doxygen \
        graphviz \
        ccache \
        pkg-config \
        curl \
        gnupg \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Install modern CMake from Kitware APT repo
RUN curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
 && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main" \
        > /etc/apt/sources.list.d/kitware.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends cmake \
 && rm -rf /var/lib/apt/lists/*

# Install yaml-cpp 0.8.0 from source to match macOS build
RUN git config --global advice.detachedHead false \
 && git clone --depth 1 --branch 0.8.0 https://github.com/jbeder/yaml-cpp.git /tmp/yaml-cpp \
 && sed -i 's/cmake_minimum_required(VERSION 3\.1)/cmake_minimum_required(VERSION 3.5)/' /tmp/yaml-cpp/CMakeLists.txt \
 && cmake -S /tmp/yaml-cpp -B /tmp/yaml-cpp/build \
        -DYAML_BUILD_SHARED_LIBS=ON \
        -DYAML_CPP_BUILD_TOOLS=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
 && cmake --build /tmp/yaml-cpp/build -j$(nproc) \
 && cmake --install /tmp/yaml-cpp/build \
 && rm -rf /tmp/yaml-cpp \
 && ldconfig

WORKDIR /workspace

CMD ["/bin/bash"]
