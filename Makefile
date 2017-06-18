CCFLAGS = -fPIC
CXXFLAGS += -std=c++11 -fPIC $$(pkg-config --cflags gtkmm-3.0)
LIBS += $$(pkg-config --libs gtkmm-3.0) -lboost_system -lboost_exception -lboost_thread -lboost_filesystem

SQLITE_FLAGS = -D_HAVE_SQLITE_CONFIG_H

ifdef DEBUG
    CXXFLAGS += -DDEBUG -ggdb3 -Wall
    CCFLAGS += -DDEBUG -ggdb3 -Wall
else
    CXXFLAGS += -DNDEBUG -O3
    CCFLAGS += -DNDEBUG -O3
endif

ddb_misc_medialib.so: sqlite3.o database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o
	$(CXX) -o ddb_misc_medialib.so -shared database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o sqlite3.o $(LIBS)

sqlite3.o: sqlite3/sqlite3.c sqlite3/sqlite3.h sqlite3/config.h
	$(CC) $(CCFLAGS) $(SQLITE_FLAGS) -c sqlite3/sqlite3.c

database.o: database.cpp database.hpp db_record.hpp
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

all: ddb_misc_medialib.so

local_install: ddb_misc_medialib.so
	mkdir -p $$HOME/.local/lib/deadbeef
	cp -f ddb_misc_medialib.so $$HOME/.local/lib/deadbeef
	
install: ddb_misc_medialib.so
	mkdir -p $(DESTDIR)/opt/deadbeef/lib/deadbeef
	cp -f ddb_misc_medialib.so $(DESTDIR)/opt/deadbeef/lib/deadbeef

clean:
	$(RM) *.o
