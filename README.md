# i2pcpp

i2pcpp is an I2P router written in C++11 by orion. It is targeted for the advanced I2P user demographic.

## Features

*This list will be populated as features are implemented.*

## Building

### Requirements

To compile, you will require the following components:

* [Botan 1.11.5][1]
* [boost 1.55.0][2]
* [cmake 2.8.11.2][3] or greater
* [sqlite 3.8.0.2][4] or greater

Optional components:

* [googletest][5] for unit testing

clang 3.3+ is the only officially supported compiler, but there is an honest effort to be compatible with g++. i2pcpp will always work on the latest version of FreeBSD.

### Using cmake

i2pcpp should be built out of source:

`mkdir build; cd build`

#### Variables

The following cmake variables are recognized:

* CMAKE_BUILD_TYPE (Debug, Release, or MinSizeRel)
* CMAKE_CXX_COMPILER
* BOOST_INCLUDEDIR
* BOOST_LIBRARYDIR
* BOTAN_INCLUDE_PREFIX
* BOTAN_LIBRARY_PREFIX
* SQLITE3_INCLUDE_PREFIX
* SQLITE3_LIBRARY_PREFIX
* WEBSOCKETPP_INCLUDE_PREFIX

If you want to enable unit testing, specify these variables:

* GTEST_INCLUDE_PREFIX
* GTEST_LIBRARY_PREFIX

Below is an example of how to invoke cmake from within your build directory:

`cmake .. -DCMAKE_BUILD_TYPE=Debug`

Specify additional variables as necessary.

#### Making

On most systems you simply need to execute `make` and everything will be built. On FreeBSD you must use `gmake`.

#### Output files

One binary, `i2p` will be produced. If you are building unit tests, a second binary `testi2p` will be produced.

## First time setup

### Database initialization

i2pcpp uses a sqlite database to store RouterInfo, profiles, and configuration data. When running i2pcpp for the first time, this database must be generated:

`./i2p --init`

This command also generates the proper crypto keys and saves them in the database.

The database will be named `i2p.db` by default. If you want to override the default database, you can specify the following argument with any command:

`./i2p --db=somefile.db`

Note that if you do not use the default database file, you must specify this argument for all commands.

### Configuration

Configuration settings are stored in the database. Individual settings are read and written in this way:

`./i2p --get key`
`./i2p --set key value`

The following keys are supported:

* ssu_bind ip (IP to bind to)
* ssu_bind_port (Port to bind to)
* ssu_external_ip (IP to advertise)
* ssu_external_port (Port to advertise)
* control_server (1 to enable, 0 to disable)
* control_server_ip (IP for the control server to bind to)
* control_server_port (Port for the control server to bind to)

### Router Info files

Router Info files generated by the Java i2p client can be imported in to i2pcpp to seed your database:

`./i2p --import someRouterInfoFile.dat`

These files are usually located in `~/.i2p/netDb/`. To recursively import the entire netDb directory, use the '--importdir' argument:

`./i2p --importdir /home/you/.i2p/netDb`

You can export your own personal Router Info file:

`./i2p --export myRouterInfoFile.dat`

This allows you to share it with others.

To wipe the database clean of all profiles and routers, use the '--wipe' option:

`./i2p --wipe`

This will *not* clear your configuration settings or crypto keys.

## Running

By default, i2pcpp prints logging output to stderr. To redirect this to a log file, use the `--log` (or `-l`) options:

`./i2p -l`

This option takes an optional argument to specify the filename to log to. If no argument is provided, the log file `i2p.log` will be used.

## Contact

The author hangs out in #i2p-dev on the irc2p network under the nickname orion. A GPG key is included in the `doc/` directory which can be used to send private messages and verify commits.

GPG fingerprint: 350E 6173 B4C5 3976 AD82  27A4 0571 6F61 2464 26E9

[1]: http://botan.randombit.net/download.html
[2]: http://www.boost.org/users/download/
[3]: http://www.cmake.org/cmake/resources/software.html
[4]: http://www.sqlite.org/download.html
[5]: http://code.google.com/p/googletest/downloads/list
