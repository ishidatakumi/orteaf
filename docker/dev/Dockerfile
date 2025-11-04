# syntax=docker/dockerfile:1.4
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Etc/UTC

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        cmake \
        ninja-build \
        git \
        python3 \
        python3-pip \
        doxygen \
        graphviz \
        ccache \
        pkg-config \
 && rm -rf /var/lib/apt/lists/*

# Default workdir for mounting the project
WORKDIR /workspace

CMD ["/bin/bash"]
