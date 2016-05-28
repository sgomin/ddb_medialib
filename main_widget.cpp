#include "main_widget.hpp"

#include "settings_dlg.hpp"
#include "medialib.h"

#include <fstream>

struct ByDirectoryColumns : Gtk::TreeModel::ColumnRecord
{
	Gtk::TreeModelColumn<RecordID>	fileId;
	Gtk::TreeModelColumn<Glib::ustring>	filename;
	ByDirectoryColumns() { add(fileId); add(filename); }
};

static const ByDirectoryColumns byDirColumns;


MainWidget::MainWidget(
        Database & db, 
        ScanEventSource scanEventSource,
        fs::path const& configDir)
 : db_(db)
 , scanEventSource_(scanEventSource)
 , settingsBtn_("Settings")
 , expandRowsFileName_((configDir / "expanded_rorws").string())
{
    styleCombo_.append("By directory structure");
    styleCombo_.set_active(0);
    
	settingsBtn_.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onSettings));
	
	firstRow_.pack_start(styleCombo_, Gtk::PACK_EXPAND_WIDGET);
	firstRow_.pack_end(settingsBtn_, Gtk::PACK_SHRINK);
	
	pTreeModel_ = Gtk::TreeStore::create(byDirColumns);
	pTreeModel_->set_sort_column(byDirColumns.filename, Gtk::SORT_ASCENDING);
	// adding mapping of root record to root row 
	file2row_.insert(std::make_pair(ROOT_RECORD_ID, 
		Gtk::TreeModel::RowReference(pTreeModel_, Gtk::TreeModel::Path("0"))));
	fillData(ROOT_RECORD_ID, pTreeModel_->children());
	setupTreeView();
    
	scrolledWindow_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledWindow_.add(treeVeiew_);
    
	sidebar_.pack_start(firstRow_, Gtk::PACK_SHRINK);
	sidebar_.pack_start(scrolledWindow_, Gtk::PACK_EXPAND_WIDGET);
    add(sidebar_);
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
	treeVeiew_.signal_row_activated().connect(
            sigc::mem_fun(*this, &MainWidget::onRowActivated));
    
    // Drag'n'drop
    std::vector<Gtk::TargetEntry> targets =
        { Gtk::TargetEntry("text/uri-list", Gtk::TARGET_SAME_APP) };
    
    treeVeiew_.drag_source_set(targets, Gdk::BUTTON1_MASK, 
        Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
    treeVeiew_.drag_dest_add_uri_targets();
    treeVeiew_.signal_drag_data_get().connect(
        sigc::mem_fun(*this, &MainWidget::onDragDataGet));
}


void MainWidget::saveExpandedRows()
{
    std::ofstream expRowsFile(expandRowsFileName_);
    
    treeVeiew_.map_expanded_rows(
    [&expRowsFile](Gtk::TreeView* tree_view, const Gtk::TreeModel::Path& path)
    {
        expRowsFile << path.to_string() << std::endl;
    });
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
	Records children = db_.children(from);
	
	for (Record const& rec : children)
	{
		Gtk::TreeModel::iterator itRow = pTreeModel_->append(to);
		
		fillRow(itRow, rec);
		
		if (rec.second.header.isDir)
		{
			fillData(rec.first, (*itRow)->children());
		}
	}
}


void MainWidget::fillRow(Gtk::TreeModel::iterator itRow, Record const& rec)
{
	(*itRow)[byDirColumns.fileId] = rec.first;
	(*itRow)[byDirColumns.filename] = 
			fs::path(rec.second.header.fileName).filename().string();

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
	
	RecordData const rec = db_.get((*itRow)[byDirColumns.fileId]);
	std::string const& fileName = rec.header.fileName;
	
	if (rec.header.isDir)
	{
		if (deadbeef->plt_add_dir2 (0, plt, fileName.c_str(), NULL, NULL) < 0)
		{
			std::cerr << "Failed to add folder '" << fileName
					<< "' to playlist" << std::endl;
		}
	}
	else if (fs::is_regular(fileName.c_str()))
	{
		if (deadbeef->plt_add_file2 (0, plt, fileName.c_str(), NULL, NULL) < 0)
        {
			std::cerr << "Failed to add file '" << fileName
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
            RecordData const rec = db_.get((*itRow)[byDirColumns.fileId]);

            if (uris.empty())
            {
                uris += ' ';
            }
            
            uris += Glib::filename_to_uri(rec.header.fileName);
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
			addRec(e.id);
			break;
			
		case ScanEvent::DELETED:
			delRec(e.id);
			break;
			
		case ScanEvent::UPDATED:
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
}


void MainWidget::addRec(const RecordID& id)
try
{
	RecordData recData = db_.get(id);
	FileToRowMap::iterator const itParentRecord = 
			file2row_.find(recData.header.parentID);
			
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