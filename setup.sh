#!/usr/bin/env bash

# Default prefix directory where everything is installed
prefix_dir="${HOME}/local"

# The number of threads for make to use
build_jobs=16

# Terminate on error
set -e

if [[ $# -eq 1 ]]; then
  prefix_dir=$1
fi

mkdir -p ${prefix_dir}
echo ${prefix_dir} > prefix_dir

# Use the newer version of gcc and g++ for any C/C++ builds
export CC=/opt/rh/devtoolset-3/root/bin/gcc
export CXX=/opt/rh/devtoolset-3/root/bin/g++

# Update path environment variable
export PATH=${prefix_dir}/bin:${PATH}

# Install cmake
if [[ ! -f cmake_done ]]; then
    minor_ver=3.11
    full_ver=${minor_ver}.0
    fname=cmake-${full_ver}
    downloadurl=https://cmake.org/files/v${minor_ver}/${fname}.tar.gz
    wget -qO- ${downloadurl} | tar xzv
    pushd ${fname}
    ./bootstrap --prefix=${prefix_dir} --parallel=${build_jobs}
    make -j ${build_jobs}
    make install
    popd
    rm -fr ${fname}
    touch cmake_done
fi

# Install mongodb
if [[ ! -f mongodb_done ]]; then
    fname=mongodb-linux-x86_64-3.6.3
    wget -qO- https://fastdl.mongodb.org/linux/${fname}.tgz | tar zxv
    pushd ${fname}
    mv bin/* ${prefix_dir}/bin/.
    popd
    rm -fr ${fname}
    touch mongodb_done
fi

# Install mongodb c client driver
if [[ ! -f mongoc_done ]]; then
    ver=1.9.3
    fname=mongo-c-driver-${ver}
    downloadurl=https://github.com/mongodb/mongo-c-driver/releases/download
    wget -qO- ${downloadurl}/${ver}/${fname}.tar.gz | tar xzv
    pushd ${fname}
    ./configure --disable-automatic-init-and-cleanup --prefix=${prefix_dir}
    make -j ${build_jobs}
    make install
    popd
    rm -fr ${fname}
    touch mongoc_done
fi

# Install mongodb c++ client driver
if [[ ! -f mongocxx_done ]]; then
    ver=r3.2.0
    fname=mongo-cxx-driver-${ver}
    downloadurl=https://github.com/mongodb/mongo-cxx-driver/archive/${ver}.tar.gz
    wget -qO- ${downloadurl} | tar xzv
    pushd ${fname}/build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${prefix_dir} ..
    make -j ${build_jobs}
    make install
    popd
    rm -fr ${fname}
    touch mongocxx_done
fi

# Install redis
if [[ ! -f redis_done ]]; then
    fname=redis-stable
    downloadurl=http://download.redis.io/${fname}.tar.gz
    wget -qO- ${downloadurl} | tar xzv
    pushd ${fname}
    make -j ${build_jobs}
    mv src/redis-server ${prefix_dir}/bin
    mv src/redis-cli ${prefix_dir}/bin
    popd
    rm -fr ${fname}
    touch redis_done
fi

# Install redis c client driver
if [[ ! -f hiredis_done ]]; then
    ver=v0.13.3
    fname=hiredis
    downloadurl=https://github.com/redis/${fname}.git
    git clone --branch ${ver} --depth 1 ${downloadurl}
    pushd ${fname}
    make -j ${build_jobs} PREFIX=${prefix_dir}
    make install PREFIX=${prefix_dir}
    popd
    rm -fr ${fname}
    touch hiredis_done
fi

# Install flask
if [[ ! -f ${prefix_dir}/bin/flask ]]; then
    pip install --prefix ${prefix_dir} flask
fi

# Update environment variables
pypackages=python3.4/site-packages
export PYTHONPATH=${prefix_dir}/lib/${pypackages}:${prefix_dir}/lib64/${pypackages}
export LD_LIBRARY_PATH=${prefix_dir}/lib:${prefix_dir}/lib64:${LD_LIBRARY_PATH}

# Create a file that can be sourced to configure the paths when one comes back
# to this project after everything has already been built and installed.
if [[ ! -f env_vars ]]; then
cat > env_vars << EOF
export PYTHONPATH=${PYTHONPATH}
export PATH=${PATH}
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
EOF
fi

# Now build the Haystack components
if [[ ! -f hay_done ]]; then
    mkdir -p build
    pushd build
    cmake -DCMAKE_PREFIX_PATH=${prefix_dir} -DCMAKE_INSTALL_PREFIX=${prefix_dir} ../src/
    make -j ${build_jobs}
    make install
    popd
    touch hay_done
fi
