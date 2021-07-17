GTKMM_VER=3.0
PLUGIN_NAME=ddb_misc_medialib
PLUGIN_FILENAME=$(PLUGIN_NAME)_gtk3.so
ARCH=$(shell uname -m)

ifdef GTK2
    GTKMM_VER=2.4
    PLUGIN_FILENAME=$(PLUGIN_NAME)_gtk2.so
endif

CCFLAGS = -fPIC -pipe
CXXFLAGS += -std=c++17 -fPIC -pipe $$(pkg-config --cflags gtkmm-$(GTKMM_VER))
LIBS += $$(pkg-config --libs gtkmm-$(GTKMM_VER)) -lboost_exception -lboost_thread

SQLITE_FLAGS = -D_HAVE_SQLITE_CONFIG_H

ifdef DEBUG
    CXXFLAGS += -DDEBUG -ggdb3 -Wall
    CCFLAGS += -DDEBUG -ggdb3 -Wall
else
    CXXFLAGS += -DNDEBUG -O3 -fomit-frame-pointer
    CCFLAGS += -DNDEBUG -O3 -fomit-frame-pointer
endif

ifdef GTK2
    CXXFLAGS += -DUSE_GTK2
endif


$(PLUGIN_FILENAME): sqlite3.o sqlite_locked.o database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o
	$(CXX) -o $(PLUGIN_FILENAME) -shared database.o sqlite_locked.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o sqlite3.o $(LIBS)

sqlite3.o: sqlite3/sqlite3.c sqlite3/sqlite3.h sqlite3/config.h
	$(CC) $(CCFLAGS) $(SQLITE_FLAGS) -c sqlite3/sqlite3.c

sqlite_locked.o: sqlite3/sqlite_locked.cpp sqlite3/sqlite_locked.h sqlite3/sqlite3.h sqlite3/config.h
	$(CXX) $(CXXFLAGS) -c sqlite3/sqlite_locked.cpp

database.o: database.cpp database.hpp db_record.hpp sqlite3/sqlite_locked.h sqlite3/sqlite3.h sqlite3/config.h
	$(CXX) $(CXXFLAGS) -c database.cpp

main_widget.o: main_widget.cpp main_widget.hpp database.hpp db_record.hpp
	$(CXX) $(CXXFLAGS) -c main_widget.cpp

medialib.o: medialib.cpp medialib.h plugin.hpp
	$(CXX) $(CXXFLAGS) -c medialib.cpp

plugin.o: plugin.cpp plugin.hpp scan_thread.hpp database.hpp db_record.hpp
	$(CXX) $(CXXFLAGS) -c plugin.cpp

scan_thread.o: scan_thread.cpp scan_thread.hpp database.hpp db_record.hpp
	$(CXX) $(CXXFLAGS) -c scan_thread.cpp

settings_dlg.o: settings_dlg.cpp settings_dlg.hpp
	$(CXX) $(CXXFLAGS) -c settings_dlg.cpp

settings.o: settings.cpp settings.hpp
	$(CXX) $(CXXFLAGS) -c settings.cpp

all: $(PLUGIN_FILENAME)

local_install: $(PLUGIN_FILENAME)
	mkdir -p $$HOME/.local/lib/deadbeef
	cp -f $(PLUGIN_FILENAME) $$HOME/.local/lib/deadbeef

install: $(PLUGIN_FILENAME)
	if [ -d $(DESTDIR)/usr/lib/$(ARCH)-linux-gnu ] ;			\
	then									\
	    mkdir -p $(DESTDIR)/usr/lib/$(ARCH)-linux-gnu/deadbeef;		\
	    cp -f $(PLUGIN_FILENAME) $(DESTDIR)/usr/lib/$(ARCH)-linux-gnu/deadbeef; \
	else									\
	    mkdir -p $(DESTDIR)/usr/lib/deadbeef;				\
	    cp -f $(PLUGIN_FILENAME) $(DESTDIR)/usr/lib/deadbeef;		\
	fi									\

clean:
	$(RM) *.o *.so
