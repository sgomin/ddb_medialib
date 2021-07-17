#ifndef MAIN_WIDGET_HPP
#define	MAIN_WIDGET_HPP

#include "plugin.hpp"
#include "database.hpp"
#include "scan_event.hpp"

#include <boost/functional/hash.hpp>

#include <filesystem>
namespace fs = std::filesystem;
#include <unordered_map>

#include <gtkmm.h>
#include <glibmm/dispatcher.h>

class MainWidget : public Gtk::EventBox
{
public:
    MainWidget(
            DbReader&& db, 
            ScanEventSource scanEventSource,
            fs::path const& configDir);
	virtual ~MainWidget() override;
    
    Glib::Dispatcher& getOnChangedDisp() { return onChangesDisp_; }
    ActiveRecordsSync & getActiveRecords() { return activeRecords_; }
    void onDisconnect();
    
private:
	// signal handlers
    void onSettings();
    void onRefresh();
    void onRowActivated(
            const Gtk::TreeModel::Path& path, 
            Gtk::TreeViewColumn* column);
    void onRowExpanded(
            const Gtk::TreeModel::iterator& iter, 
            const Gtk::TreeModel::Path& path);
    void onRowCollapsed(
            const Gtk::TreeModel::iterator& iter, 
            const Gtk::TreeModel::Path& path);
    void onDragDataGet(
            const Glib::RefPtr<Gdk::DragContext>& context,
            Gtk::SelectionData& selection_data, 
            guint info, 
            guint time);
	void onChanged();
    
    // auxiliary functions
    void fillData(const RecordID& from, const Gtk::TreeModel::Children& to);
    void setupTreeView();
    void fillRow(Gtk::TreeModel::iterator itRow, FileRecord const& rec);
    void delRec(const RecordID& id);
    void addRec(const RecordID& id);
    void onPreDeleteRow(Gtk::TreeModel::Row const& row);
    void saveExpandedRows();
    void restoreExpandedRows();
    
    typedef std::unordered_map<
            RecordID, 
            Gtk::TreeModel::RowReference, 
            boost::hash<RecordID>> FileToRowMap;
	
    DbReader                        db_;
    ScanEventSource                 scanEventSource_;
    Gtk::TreeView                   treeVeiew_;
    Glib::RefPtr<Gtk::TreeStore>    pTreeModel_;
    FileToRowMap                    file2row_;
    Glib::Dispatcher                onChangesDisp_;
    sigc::connection                changeConnection_;
    std::string const               expandRowsFileName_;
    ActiveRecordsSync               activeRecords_;
};


#endif	/* MAIN_WIDGET_HPP */

