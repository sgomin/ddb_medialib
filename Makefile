CFLAGS = -std=c++11 -fPIC $$(pkg-config --cflags glibmm-2.4 gtkmm-3.0)
LDFLAGS = $$(pkg-config --libs glibmm-2.4 gtkmm-3.0) -lboost_system -lboost_exception -ldb_cxx -lboost_filesystem

ddb_misc_medialib.so: database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o
	g++ $(LDFLAGS) -o ddb_misc_medialib.so -shared database.o main_widget.o medialib.o plugin.o scan_thread.o settings_dlg.o

database.o: database.cpp database.hpp
	g++ $(CFLAGS) -c database.cpp

main_widget.o: main_widget.cpp main_widget.hpp
	g++ $(CFLAGS) -c main_widget.cpp

medialib.o: medialib.cpp medialib.h
	g++ $(CFLAGS) -c medialib.cpp

plugin.o: plugin.cpp plugin.h
	g++ $(CFLAGS) -c plugin.cpp

scan_thread.o: scan_thread.cpp scan_thread.hpp
	g++ $(CFLAGS) -c scan_thread.cpp

settings_dlg.o: settings_dlg.cpp settings_dlg.hpp
	g++ $(CFLAGS) -c settings_dlg.cpp

all: ddb_misc_medialib.so

clean:
	rm *.o
