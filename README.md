# ddb_medialib

Media library plugin for Deadbeef music player

## Dependencies

- libgtkmm-3.0-dev / libgtkmm-2.4-dev
- libboost-dev
- libboost-system-dev
- libboost-thread-dev
- libboost-exception-dev
- libboost-filesystem-dev

## Building

1. Obtain deadbeef.h, gtkui_api.h from [Deadbeef repo](https://github.com/DeaDBeeF-Player/deadbeef) and put it in `deadbeef` directory
2. Build `make all` or `make GTK2=1 all`, the latter builds GTK2 version
3. Install `make install` or `make GTK2=1 install`
4. Alternatively, install for current user `make local_install`
