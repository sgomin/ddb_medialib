#ifndef MAIN_WIDGET_HPP
#define	MAIN_WIDGET_HPP

#include "plugin.h"
#include "database.hpp"

#include <gtkmm.h>

class MainWidget : public Gtk::EventBox
{
public:
    MainWidget(Database & db);
    
private:
    void onSettings();
	void fillData(const RecordID& from, const Gtk::TreeModel::Children& to);
    
	Database &						db_;
    Gtk::VBox						sidebar_;
	Glib::RefPtr<Gtk::TreeStore>	pTreeModel_;
};


#endif	/* MAIN_WIDGET_HPP */

