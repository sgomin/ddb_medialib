CXXFLAGS += -I$(DEADBEEF_DIR) -std=c++11 -fPIC $$(pkg-config --cflags gtkmm-3.0)
LIBS += $$(pkg-config --libs gtkmm-3.0) -ldb_cxx -lboost_system -lboost_exception -lboost_filesystem

ifdef DEBUG
    CXXFLAGS += -DDEBUG -ggdb3 -Wall
else
    CXXFLAGS += -DNDEBUG -O3
endif

ddb_misc_medialib.so: db_record.o database.o db_iterator.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o
	$(CXX) -o ddb_misc_medialib.so -shared db_record.o database.o db_iterator.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o settings.o $(LIBS)

db_record.o: db_record.cpp db_record.hpp
	$(CXX) $(CXXFLAGS) -Wno-deprecated -c db_record.cpp

database.o: database.cpp database.hpp db_iterator.hpp db_record.hpp
	$(CXX) $(CXXFLAGS) -Wno-deprecated -c database.cpp
	
db_iterator.o: db_iterator.cpp db_iterator.hpp db_record.hpp
	$(CXX) $(CXXFLAGS) -c db_iterator.cpp

main_widget.o: main_widget.cpp main_widget.hpp event_queue.hpp
	$(CXX) $(CXXFLAGS) -c main_widget.cpp

medialib.o: medialib.cpp medialib.h
	$(CXX) $(CXXFLAGS) -c medialib.cpp

plugin.o: plugin.cpp plugin.hpp
	$(CXX) $(CXXFLAGS) -c plugin.cpp

scan_thread.o: scan_thread.cpp scan_thread.hpp event_queue.hpp db_iterator.hpp
	$(CXX) $(CXXFLAGS) -c scan_thread.cpp

settings_dlg.o: settings_dlg.cpp settings_dlg.hpp
	$(CXX) $(CXXFLAGS) -c settings_dlg.cpp

settings.o: settings.cpp settings.hpp
	$(CXX) $(CXXFLAGS) -c settings.cpp

all: ddb_misc_medialib.so

install: ddb_misc_medialib.so
	mkdir -p $$HOME/.local/lib/deadbeef
	cp -f ddb_misc_medialib.so $$HOME/.local/lib/deadbeef

clean:
	$(RM) *.o
