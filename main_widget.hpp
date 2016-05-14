#ifndef MAIN_WIDGET_HPP
#define	MAIN_WIDGET_HPP

#include "plugin.hpp"
#include "database.hpp"
#include "scan_event.hpp"

#include <boost/functional/hash.hpp>

#include <unordered_map>

#include <gtkmm.h>

class MainWidget : public Gtk::EventBox
{
public:
    MainWidget(Database & db, ScanEventSource scanEventSource);
	~MainWidget() override;
    
	
private:
	// signal handlers
    void onSettings();
	void onRowActivated(
			const Gtk::TreeModel::Path& path, 
			Gtk::TreeViewColumn* column);
	bool onIdle();
	
	// auxiliary functions
	void fillData(const RecordID& from, const Gtk::TreeModel::Children& to);
	void fillRow(Gtk::TreeModel::iterator itRow, Record const& rec);
	void delRec(const RecordID& id);
	void addRec(const RecordID& id);
	void onPreDeleteRow(Gtk::TreeModel::Row const& row);
    
	typedef std::unordered_map<
		RecordID, 
		Gtk::TreeModel::RowReference, 
		boost::hash<RecordID>> FileToRowMap;
	
	Database &						db_;
    ScanEventSource                 scanEventSource_;
    Gtk::VBox						sidebar_;
	Gtk::HBox						firstRow_;
	Gtk::ComboBoxText				styleCombo_;
	Gtk::Button						settingsBtn_;
	Gtk::ScrolledWindow				scrolledWindow_;
	Gtk::TreeView					treeVeiew_;
	Glib::RefPtr<Gtk::TreeStore>	pTreeModel_;
	FileToRowMap					file2row_;
	sigc::connection				idleConnection_;
};


#endif	/* MAIN_WIDGET_HPP */

