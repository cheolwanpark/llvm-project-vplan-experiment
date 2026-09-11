# Build with Docker BuildKit. External packages are not an archived image digest.
FROM ubuntu:24.04 AS llvm-build
ARG DEBIAN_FRONTEND=noninteractive
ARG LLVM_JOBS=2
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential clang cmake git lld ninja-build python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
RUN git init llvm-project \
    && git -C llvm-project remote add origin https://github.com/cheolwanpark/llvm-project.git \
    && git -C llvm-project fetch --depth 1 origin 89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb \
    && git -C llvm-project checkout --detach FETCH_HEAD \
    && test "$(git -C llvm-project rev-parse HEAD)" = 89ebea3545acc3c30b4fb90c1f3ed7f0f6feeddb
RUN cmake -S llvm-project/llvm -B llvm-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCMAKE_INSTALL_PREFIX=/opt/llvm-main -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_DUMP=ON \
    -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD=RISCV \
    -DLLVM_USE_LINKER=lld -DLLVM_PARALLEL_LINK_JOBS=1 \
    -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF \
    && cmake --build llvm-build --target install-clang install-clang-resource-headers \
       install-lld install-opt install-llc install-llvm-objdump -j "${LLVM_JOBS}" \
    && ln -sfn clang /opt/llvm-main/bin/clang++ \
    && ln -sfn lld /opt/llvm-main/bin/ld.lld

FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates wget python3 gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf \
    && wget -qO /usr/share/keyrings/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
    && echo "deb [signed-by=/usr/share/keyrings/apt.llvm.org.asc] http://apt.llvm.org/noble/ llvm-toolchain-noble-22 main" \
       > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends libc++-22-dev \
    && rm -rf /var/lib/apt/lists/*
COPY --from=llvm-build /opt/llvm-main /opt/llvm-main
COPY --from=llvm-build /workspace/llvm-project/llvm/LICENSE.TXT /opt/llvm-main/LICENSE.TXT
RUN mkdir -p /opt/llvm-main/include \
    && ln -sfn /usr/lib/llvm-22/include/c++ /opt/llvm-main/include/c++ \
    && for config in /usr/lib/llvm-22/include/c++/v1/__config_site /usr/include/c++/v1/__config_site; do \
      if test -f "$config"; then \
        sed -i \
          -e 's/#define _LIBCPP_HAS_THREADS 1/#define _LIBCPP_HAS_THREADS 0/' \
          -e 's/#define _LIBCPP_HAS_MONOTONIC_CLOCK 1/#define _LIBCPP_HAS_MONOTONIC_CLOCK 0/' \
          -e 's/#define _LIBCPP_HAS_THREAD_API_PTHREAD 1/#define _LIBCPP_HAS_THREAD_API_PTHREAD 0/' \
          -e 's/#define _LIBCPP_HAS_TERMINAL 1/#define _LIBCPP_HAS_TERMINAL 0/' \
          -e 's/#define _LIBCPP_HAS_FILESYSTEM 1/#define _LIBCPP_HAS_FILESYSTEM 0/' \
          -e 's/#define _LIBCPP_HAS_RANDOM_DEVICE 1/#define _LIBCPP_HAS_RANDOM_DEVICE 0/' \
          -e 's/#define _LIBCPP_HAS_LOCALIZATION 1/#define _LIBCPP_HAS_LOCALIZATION 0/' \
          -e 's/#define _LIBCPP_HAS_UNICODE 1/#define _LIBCPP_HAS_UNICODE 0/' \
          -e 's/#define _LIBCPP_HAS_WIDE_CHARACTERS 1/#define _LIBCPP_HAS_WIDE_CHARACTERS 0/' \
          -e 's/#define _LIBCPP_HAS_TIME_ZONE_DATABASE 1/#define _LIBCPP_HAS_TIME_ZONE_DATABASE 0/' \
          -e 's/#define _LIBCPP_LIBC_PICOLIBC 0/#define _LIBCPP_LIBC_PICOLIBC 1/' "$config"; \
      fi; \
    done
ENV PATH=/opt/llvm-main/bin:${PATH}
WORKDIR /work
