#include "main_widget.hpp"

#include "settings_dlg.hpp"
#include "medialib.h"

#include <fstream>
#include <iostream>

struct ByDirectoryColumns : Gtk::TreeModel::ColumnRecord
{
	Gtk::TreeModelColumn<RecordID>	fileId;
	Gtk::TreeModelColumn<Glib::ustring>	filename;
	ByDirectoryColumns() { add(fileId); add(filename); }
};

static const ByDirectoryColumns byDirColumns; // TODO: make non-static


MainWidget::MainWidget(
        DbReader&& db, 
        ScanEventSource scanEventSource,
        fs::path const& configDir)
 : db_(std::move(db))
 , scanEventSource_(scanEventSource)
 , expandRowsFileName_((configDir / "expanded_rows").string())
{
    // "mode" combo
    auto pModeCombo = Gtk::manage(new Gtk::ComboBoxText());
    pModeCombo->set_tooltip_text("Display mode");
    pModeCombo->append("By directory structure");
    pModeCombo->set_active(0);
    
    // button "Refresh"
    auto pRefreshImg = Gtk::manage(
        new Gtk::Image(Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON));
    auto pBtnRefresh = Gtk::manage(new Gtk::Button());
    pBtnRefresh->set_image(*pRefreshImg);
    pBtnRefresh->set_tooltip_text("Refresh");
	pBtnRefresh->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onRefresh));
    
    // button "Properties"
    auto pSettingsImg = Gtk::manage(
            new Gtk::Image(Gtk::Stock::PROPERTIES, Gtk::ICON_SIZE_BUTTON));
    auto pBtnSettings = Gtk::manage(new Gtk::Button());
    pBtnSettings->set_image(*pSettingsImg);
    pBtnSettings->set_tooltip_text("Properties");
	pBtnSettings->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onSettings));
    
    // row with "mode" combo, "refresh" and "properties" buttons
    auto pPirstRow = Gtk::manage(new Gtk::HBox());
	pPirstRow->pack_start(*pModeCombo, Gtk::PACK_EXPAND_WIDGET);
    pPirstRow->pack_end(*pBtnSettings, Gtk::PACK_SHRINK);
    pPirstRow->pack_end(*pBtnRefresh, Gtk::PACK_SHRINK);
	
	pTreeModel_ = Gtk::TreeStore::create(byDirColumns);
	pTreeModel_->set_sort_column(byDirColumns.filename, Gtk::SORT_ASCENDING);
	// adding mapping of root record to root row 
	file2row_.insert(std::make_pair(ROOT_RECORD_ID, 
		Gtk::TreeModel::RowReference(pTreeModel_, Gtk::TreeModel::Path("0"))));
	fillData(ROOT_RECORD_ID, pTreeModel_->children());
	setupTreeView();
    
    // main window containing the tree view
    auto pScrolledWindow = Gtk::manage(new Gtk::ScrolledWindow());
	pScrolledWindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    pScrolledWindow->add(treeVeiew_);
    
    // entire plugin widget
    auto pSideBar = Gtk::manage(new Gtk::VBox());
	pSideBar->pack_start(*pPirstRow, Gtk::PACK_SHRINK);
	pSideBar->pack_start(*pScrolledWindow, Gtk::PACK_EXPAND_WIDGET);
    add(*pSideBar);
    restoreExpandedRows();
    show_all();
	
	changeConnection_ = onChangesDisp_.connect(
			sigc::mem_fun(*this, &MainWidget::onChanged));
}


MainWidget::~MainWidget()
{
    changeConnection_.disconnect();
    saveExpandedRows();
}


void MainWidget::setupTreeView()
{
    treeVeiew_.set_model(pTreeModel_);
	treeVeiew_.append_column("File Name", byDirColumns.filename);
	treeVeiew_.set_headers_visible(false);
// TODO: support multi-line drag'n'drop 
//    treeVeiew_.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	treeVeiew_.signal_row_activated().connect(
        sigc::mem_fun(*this, &MainWidget::onRowActivated));
    treeVeiew_.signal_row_expanded().connect(
        sigc::mem_fun(*this, &MainWidget::onRowExpanded));
    treeVeiew_.signal_row_collapsed().connect(
        sigc::mem_fun(*this, &MainWidget::onRowCollapsed));
    
    // Drag'n'drop
    std::vector<Gtk::TargetEntry> targets =
        { Gtk::TargetEntry("text/uri-list", 
                Gtk::TARGET_SAME_APP | Gtk::TARGET_OTHER_WIDGET) };
    
    treeVeiew_.enable_model_drag_source(targets);
    treeVeiew_.signal_drag_data_get().connect(
        sigc::mem_fun(*this, &MainWidget::onDragDataGet));
}


void MainWidget::saveExpandedRows()
try
{
    std::ofstream expRowsFile(expandRowsFileName_);
    
    treeVeiew_.map_expanded_rows(
    [&expRowsFile](Gtk::TreeView* tree_view, const Gtk::TreeModel::Path& path)
    {
        expRowsFile << path.to_string() << std::endl;
    });
}
catch(std::exception const& e)
{
	std::cerr << "Failed to save expanded rows: " << e.what() << std::endl;
}


void MainWidget::restoreExpandedRows()
{
    std::ifstream expRowsFile(expandRowsFileName_);
    char buff[256];
    
    while (expRowsFile.getline(buff, sizeof(buff)))
    {
        Glib::ustring const str(buff, expRowsFile.gcount() - 1);
        Gtk::TreeModel::Path const path(str);
        treeVeiew_.expand_row(path, /*expand all*/false);
    }
}


void MainWidget::onDisconnect()
{
    saveExpandedRows();
}


void MainWidget::onSettings()
{
    Settings settings = Plugin::getSettings();
    SettingsDlg dlgSettings(settings);
    
    if (dlgSettings.run() == Gtk::RESPONSE_OK)
    {
        Plugin::storeSettings(std::move(settings));
    }
}


void MainWidget::fillData(
		const RecordID& from, const Gtk::TreeModel::Children& to)
{
    std::set<RecordID> subdirIDs;
    
	for (FileRecord const& rec : db_.childrenFiles(from))
	{
		Gtk::TreeModel::iterator itRow = pTreeModel_->append(to);
		
		fillRow(itRow, rec);
		
		if (rec.second.isDir)
		{
			subdirIDs.insert(rec.first);
		}
	}
    
    for (auto itRow = to.begin(); itRow != to.end(); ++itRow)
    {
        RecordID id = (*itRow)[byDirColumns.fileId];
        
        if (subdirIDs.count(id))
        {
            fillData(id, (*itRow)->children());
        }
    }
}


void MainWidget::fillRow(Gtk::TreeModel::iterator itRow, FileRecord const& rec)
{
	(*itRow)[byDirColumns.fileId] = rec.first;
	(*itRow)[byDirColumns.filename] = 
			fs::path(rec.second.fileName).filename().string();

	Gtk::TreeModel::Path path = pTreeModel_->get_path(itRow);

	file2row_.insert(std::make_pair(rec.first, 
		Gtk::TreeModel::RowReference(pTreeModel_, std::move(path))));
}


void MainWidget::onRowActivated(
		const Gtk::TreeModel::Path& path, 
		Gtk::TreeViewColumn* /*column*/)
try
{
	Gtk::TreeModel::iterator itRow = pTreeModel_->get_iter(path);
	
	if (!itRow)
	{
		return;
	}
	
	struct LockPlayLists
	{
		LockPlayLists() { deadbeef->pl_lock(); }
		~LockPlayLists() { deadbeef->pl_unlock(); }
	} lockPlaylists;
	
	ddb_playlist_t* plt = deadbeef->plt_get_curr();
	
	if (!plt)
	{
		int idx = deadbeef->plt_add(deadbeef->plt_get_count(), "New");
		deadbeef->plt_set_curr_idx(idx);
		plt = deadbeef->plt_get_curr();
	}
	
	assert(plt);
	
	struct LockPlayList
	{
		LockPlayList(ddb_playlist_t* plt) : plt_(plt)
		{
			if (deadbeef->plt_add_files_begin (plt, 0) < 0)
			{
				throw std::runtime_error(
						"could not add files to playlist (lock failed)");
			}
		}
		
		~LockPlayList()
		{
			deadbeef->plt_add_files_end(plt_, 0);
			deadbeef->plt_modified (plt_);
			deadbeef->plt_save_config (plt_);
			deadbeef->conf_save ();
			deadbeef->plt_unref(plt_);
		}
		
		ddb_playlist_t* const plt_;
	} lockPlaylist(plt);
	
	FileInfo const rec = db_.getFile((*itRow)[byDirColumns.fileId]);
	
	if (rec.isDir)
	{
		if (deadbeef->plt_add_dir2 (0, plt, rec.fileName.c_str(), NULL, NULL) < 0)
		{
			std::cerr << "Failed to add folder '" << rec.fileName
					<< "' to playlist" << std::endl;
		}
	}
	else if (fs::is_regular_file(rec.fileName.c_str()))
	{
		if (deadbeef->plt_add_file2 (0, plt, rec.fileName.c_str(), NULL, NULL) < 0)
        {
			std::cerr << "Failed to add file '" << rec.fileName
					<< "' to playlist" << std::endl;
		}
	}
}
catch(const std::exception& e)
{
	std::cerr << "Failed to add file to playlist: " << e.what() << std::endl;
}


void MainWidget::onDragDataGet(
        const Glib::RefPtr<Gdk::DragContext>& context,
        Gtk::SelectionData& selection_data, 
        guint /*info*/, 
        guint /*time*/)
try
{
    Glib::RefPtr<Gtk::TreeSelection> pSelection = treeVeiew_.get_selection();
    Glib::ustring uris;
    
    pSelection->selected_foreach_iter(
        [this, &uris](const Gtk::TreeModel::iterator& itRow)
        {
            FileInfo const rec = db_.getFile((*itRow)[byDirColumns.fileId]);

            if (!uris.empty())
            {
                uris += ' ';
            }
            
            uris += Glib::filename_to_uri(rec.fileName);
        });
        
    selection_data.set(selection_data.get_target(), uris);
}
catch(const Glib::Exception& e)
{
	std::cerr << "Failed to drag'n'drop file to playlist: " << e.what() << std::endl;
}


void MainWidget::onChanged()
{
	while (!scanEventSource_.empty())
	{
		ScanEvent e = scanEventSource_.pull();
		
		switch(e.type)
		{
		case ScanEvent::ADDED:
            std::clog << "[Widget] onAdded " << e.id << std::endl;
			addRec(e.id);
			break;
			
		case ScanEvent::DELETED:
            std::clog << "[Widget] onDeleted " << e.id << std::endl;
			delRec(e.id);
			break;
			
		case ScanEvent::UPDATED:
            std::clog << "[Widget] onUpdated " << e.id << std::endl;
			break;
		}
	}
}

void MainWidget::delRec(const RecordID& id)
{
	FileToRowMap::iterator itRecord = file2row_.find(id);
			
	if (itRecord == file2row_.end())
	{
		assert(false);
		return;
	}
	
	assert(itRecord->second.is_valid());
	
	Gtk::TreeModel::Path const path = itRecord->second.get_path();
	Gtk::TreeModel::iterator const itRow = pTreeModel_->get_iter(path);

	onPreDeleteRow(*itRow);
	pTreeModel_->erase(itRow);
}


void MainWidget::onPreDeleteRow(Gtk::TreeModel::Row const& row)
{
	for (Gtk::TreeModel::Row const& child : row.children())
	{
		onPreDeleteRow(child);
	}
	
	const RecordID& id = row[byDirColumns.fileId];
	file2row_.erase(id);
    activeRecords_->ids.erase(id);
}


void MainWidget::addRec(const RecordID& id)
try
{
	FileInfo recData = db_.getFile(id);
	FileToRowMap::iterator const itParentRecord = 
			file2row_.find(recData.parentID);
			
	if (itParentRecord == file2row_.end())
	{
		assert(false);
		throw std::runtime_error("Can't find parent row");
	}
	
	Gtk::TreeModel::Path const parentPath = itParentRecord->second.get_path();
	Gtk::TreeModel::iterator const itParentRow = pTreeModel_->get_iter(parentPath);
	Gtk::TreeModel::iterator const itNewRow = pTreeModel_->append(itParentRow->children());
	fillRow(itNewRow, make_Record(id, std::move(recData)));
}
catch(std::exception const& e)
{
	std::cerr << "Failed to add record: " << e.what() << std::endl;
}


void MainWidget::onRowExpanded(
    const Gtk::TreeModel::iterator& iter, 
    const Gtk::TreeModel::Path& path)
{
    auto fileId = (*iter)[byDirColumns.fileId];
    std::clog << "[Widget] onRowExpanded " << fileId << std::endl;
    
    auto locked = activeRecords_.synchronize();
    
    if (locked->ids.insert(fileId).second && 
        locked->onChanged)
    {
        locked->onChanged(/*restart*/false);
    }
}


namespace
{
    
bool processCollapsedChildren(
    const Gtk::TreeModel::iterator& iter,
    RecordIDs& expanded)
{
    bool changed = false;
    
    for (Gtk::TreeModel::Row const& child : iter->children())
    {
        auto const childId = child[ byDirColumns.fileId ];
        changed = expanded.erase(childId) || changed;
        changed = processCollapsedChildren(child, expanded) || changed;
    }
    
    return changed;
}
    
} // end of anonymous namespace


void MainWidget::onRowCollapsed(
    const Gtk::TreeModel::iterator& iter, 
    const Gtk::TreeModel::Path& path)
{
    auto fileId = (*iter)[ byDirColumns.fileId ];
    std::clog << "[Widget] onRowCollapsed " << fileId << std::endl;
    
    auto locked = activeRecords_.synchronize();
    bool changed = locked->ids.erase(fileId);
    
    changed = processCollapsedChildren(iter, locked->ids) || changed; 
    
    if (changed && locked->onChanged)
    {
        locked->onChanged(/*restart*/false);
    }
}

void MainWidget::onRefresh()
{
    auto locked = activeRecords_.synchronize();
    
    if (locked->onChanged)
    {
        locked->onChanged(/*restart*/true);
    }
}