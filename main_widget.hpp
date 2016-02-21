#ifndef MAIN_WIDGET_HPP
#define	MAIN_WIDGET_HPP

#include "plugin.hpp"
#include "database.hpp"

#include <boost/functional/hash.hpp>

#include <unordered_map>

#include <gtkmm.h>

class MainWidget : public Gtk::EventBox
{
public:
    MainWidget(Database & db);
    
private:
    void onSettings();
	void onRowActivated(
			const Gtk::TreeModel::Path& path, 
			Gtk::TreeViewColumn* column);
	void fillData(const RecordID& from, const Gtk::TreeModel::Children& to);
    
	typedef std::unordered_multimap<RecordID, 
									Gtk::TreeModel::RowReference, 
									boost::hash<RecordID>>
		FileToRecordMap;
	
	Database &						db_;
    Gtk::VBox						sidebar_;
	Gtk::HBox						firstRow_;
	Gtk::ComboBoxText				styleCombo_;
	Gtk::Button						settingsBtn_;
	Gtk::ScrolledWindow				scrolledWindow_;
	Gtk::TreeView					treeVeiew_;
	Glib::RefPtr<Gtk::TreeStore>	pTreeModel_;
	FileToRecordMap					file2record_;
};


#endif	/* MAIN_WIDGET_HPP */

