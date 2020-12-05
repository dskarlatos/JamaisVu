FROM ubuntu:16.04
LABEL maintainer="neil@zzrcxb.me"
LABEL description="a unified environment for compiling gem5"

RUN apt update && \
    apt install -y --no-install-recommends \
    wget \
    curl \
    vim \
    build-essential \
    git \
    m4 \
    scons \
    zlib1g \
    zlib1g-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libprotoc-dev \
    libgoogle-perftools-dev \
    python-dev \
    python-pip \
    gosu \
    sudo \
    git \
    cmake \
    python

COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN pip install six
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD /bin/bash
