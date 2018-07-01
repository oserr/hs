#!/usr/bin/env bash
#
# Runs all the services on the same host

# Address info
# redis  - 51234
# cache  - 51236
# server - 51239
# mongo  - 51235
# dir    - 51237
# store  - 51238

# Crate the directory for MongoDB
mongo_dir=/tmp/mongo/data/db
rm -fr /tmp/mongo
mkdir -p ${mongo_dir}

# Crate the directory for Haystack
haydir=/tmp/hay
rm -fr ${haydir}
mkdir -p ${haydir}

mongod --port 51235 --dbpath ${mongo_dir} &> /dev/null &
redis-server --port 51234 &> /dev/null &
cache_app 0.0.0.0 51236 0.0.0.0 51234 0.0.0.0 51238 &
dir_app 0.0.0.0 51237 mongodb://0.0.0.0:51235 0.0.0.0 51238 &
store_app 0.0.0.0 51238 ${haydir} &
python src/proxy/proxy.py --ip 0.0.0.0 --port 51239 --dirip 0.0.0.0 --dirport 51237 --cacheip 0.0.0.0 --cacheport 51236 &
