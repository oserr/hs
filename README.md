# My Haystack

A basic implementation of Facebook's Haystack. The following sections contain
instructions for setting up the environment, building the package, running
unit/integration tests, and running a demo.

## Environment
To setup the environment, simply run the following two commands:

```{bash}
setup.sh
. env_vars
```

The first command installs/builds all the dependencies into ``${HOME}/local``.
If you want everything to be installed somewhere else, then you can specify the
prefix path, e.g., ``setup.sh ${HOME}/opt``.  If you do provide a different
prefix path, then it must be an absolute path. The following
packages/applications are installed or built by ``setup.sh``.

* cmake
* mongodb
* mongodb C client driver
* mongodb C++ client driver
* redis
* redis C client driver
* flask
* haystack - these are the haystack components I wrote

*WARNING*: the script takes about 10-13 minutes to install/build everything, and
it is important to make sure that you have enough disk space, so it might be a
good choice to first clean your personal Andrew space before proceeding with
this.

``env_vars`` is a file that contains various definitions for different path
configurations, e.g., PATH, so it must be sourced in order to be able to run the
unit tests and demo correctly.

## Unit Tests
To run the unit tests, you can run the ``test_all`` executable, which you can do
as follows:

```{bash}
./build/test/test_all
```

or alternatively

```{bash}
cd build/test
./test_all
```

One of the tests launches Redis and MongoDB, so the definitions in ``env_vars``
must be in the environment for the test to pass.

## Demo
To run a demo, first make sure the definitions in ``env_vars`` are in the
environment, then launch all of the services by running ``start_all.sh``. To
access the service, simply enter a URL of the form <unix.andrew.cmu.edu>:51239
on your favorite browser, where <unix.andrew.cmu.edu> is the specific Andrew
Unix machine the service was launched from, e.g., unix7.andrew.cmu.edu. This
will give you a very bare bones page with a *Choose File* and *Upload* buttons.
Select a file to upload, and then go back to the root page, which should now
include a needle with *Show* and *Delete* links. Clicking on *Show* will show
the contents of the file that was uploaded, and clicking on *Delete* will delete
the needle from the Directory and the Store.

The front end web server is not implemented well, so it may be necessary to
refresh any of the pages visited to get the full contents. Also, the contents of
whatever is uploaded are simply dumped on the HTML, so it may be preferable to
upload plain text files.

## Haystack components
The Haystack components are the same as those presented in the Facebook paper:

* *Directory*: the executable corresponding to this component is ``dir_app.cc``.
  The Directory receives requests from the front-end, and communicates with the
  Store when it creates a new needle.
* *Cache*: The Cache receives requests from the front-end, communicates with the
  Redis cache, and makes requests from the Store.
* *Store*: The store receives needles from the Directory, and receives requests
  from the Cache.

## Files

* ``setup.sh``: installs and builds the package.
* ``start_all.sh``: launches all of the components for a demo.
* ``src``: my own components
  * ``haystack``: the haystack components
  * ``proxy``: the front-end for the demo
  * ``test``: unit/integration tests for the haystack components

## Notes
These are for my own personal use.

### Request URL

``http://<CDN>/<Cache>/<Machine ID>/<Logical volume, photo>``

### Directory
* Maps logical volumes to physical volumes.
* Load balances writes across logical volumes.
* Load balances reads across physical volumes.
* Marks some volumes as write enabled, others only read-enabled.
* Writes go through directory, because it chooses where the write is done.

### Cache
* Distributed hash table.
* If object is not located here, then it fetches item from Store.

### Store
* Contains files ``/hay/haystack/<logical volume, object id>``,
  each of which contains many photos.
* Each machine keeps open file descriptors for each file that it manages.
* Maintains in-memory mapping of photo IDs to filesystem metadata, including
  * file name,
  * file offset,
  * flags (e.g., is it deleted)
  * size in bytes.

## Setting up environment

### Installing and using environment with flask

1. Install virtualenv in home directory: ``pip install --user virtualenv``.
2. Create virtualenv project directory: ``virtualenv proj``.
3. Activate environment: ``source proj/bin/activate``.
4. Install flask in virtual environment: ``pip install flask``. Now flask can be
   used from current environment.
5. Finish using environment: ``deactivate``.
6. Remove environment: ``rm -fr proj``.

### Installing MongoDB C driver
For details, see instructions [here][1].
* run ``install_mongo_cdriver.sh``

### Unorthodox directories for installations
If you ever happen to want to link against installed libraries in a given
directory, LIBDIR, you must either use libtool, and specify the full pathname of
the library, or use the ``-LLIBDIR`` flag during linking and do at least one of
the following:
* add LIBDIR to the ``LD_LIBRARY_PATH`` environment variable during execution
* add LIBDIR to the ``LD_RUN_PATH`` environment variable during linking
* use the ``-Wl,-rpath -Wl,LIBDIR`` linker flag
* have your system administrator add LIBDIR to ``/etc/ld.so.conf``

[1]: http://mongoc.org/libmongoc/current/installing.html
