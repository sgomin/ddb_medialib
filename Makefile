CXXFLAGS += -std=c++11 -fPIC $$(pkg-config --cflags glibmm-2.4 gtkmm-3.0)
LDFLAGS += $$(pkg-config --libs glibmm-2.4 gtkmm-3.0) -lboost_system -lboost_exception -ldb_cxx -lboost_filesystem

ifdef DEBUG
    CXXFLAGS += -DDEBUG -ggdb3 -Wall
else
    CXXFLAGS += -DNDEBUG -O3
endif

ddb_misc_medialib.so: database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o
	$(CXX) $(LDFLAGS) -o ddb_misc_medialib.so -shared database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o

database.o: database.cpp database.hpp
	$(CXX) $(CXXFLAGS) -Wno-deprecated -c database.cpp

main_widget.o: main_widget.cpp main_widget.hpp
	$(CXX) $(CXXFLAGS) -c main_widget.cpp

medialib.o: medialib.cpp medialib.h
	$(CXX) $(CXXFLAGS) -c medialib.cpp

plugin.o: plugin.cpp plugin.hpp
	$(CXX) $(CXXFLAGS) -c plugin.cpp

scan_thread.o: scan_thread.cpp scan_thread.hpp
	$(CXX) $(CXXFLAGS) -c scan_thread.cpp

settings_dlg.o: settings_dlg.cpp settings_dlg.hpp
	$(CXX) $(CXXFLAGS) -c settings_dlg.cpp

all: ddb_misc_medialib.so

install: ddb_misc_medialib.so
	mkdir -p $$HOME/.local/lib/deadbeef
	cp -f ddb_misc_medialib.so $$HOME/.local/lib/deadbeef

clean:
	$(RM) *.o
