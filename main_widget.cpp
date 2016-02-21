#include "main_widget.hpp"

#include "settings_dlg.hpp"
#include "medialib.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

struct ByDirectoryColumns : Gtk::TreeModel::ColumnRecord
{
	Gtk::TreeModelColumn<RecordID>	fileId;
	Gtk::TreeModelColumn<Glib::ustring>	filename;
	ByDirectoryColumns() { add(fileId); add(filename); }
};

static const ByDirectoryColumns byDirColumns;

MainWidget::MainWidget(Database & db)
 : db_(db)
 , settingsBtn_("Settings")
{
    styleCombo_.append("By directory structure");
    styleCombo_.set_active(0);
    
	settingsBtn_.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onSettings));
	
	firstRow_.pack_start(styleCombo_, Gtk::PACK_EXPAND_WIDGET);
	firstRow_.pack_end(settingsBtn_, Gtk::PACK_SHRINK);
	
	pTreeModel_ = Gtk::TreeStore::create(byDirColumns);
	pTreeModel_->set_sort_column(byDirColumns.filename, Gtk::SORT_ASCENDING);
	fillData(ROOT_RECORD_ID, pTreeModel_->children());
	treeVeiew_.set_model(pTreeModel_);
	treeVeiew_.append_column("File Name", byDirColumns.filename);
	treeVeiew_.set_headers_visible(false);
	treeVeiew_.signal_row_activated().connect(
            sigc::mem_fun(*this, &MainWidget::onRowActivated));
    
	scrolledWindow_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledWindow_.add(treeVeiew_);
    
	sidebar_.pack_start(firstRow_, Gtk::PACK_SHRINK);
	sidebar_.pack_start(scrolledWindow_, Gtk::PACK_EXPAND_WIDGET);
    add(sidebar_);
    show_all();
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
	
	for (Record& rec : children)
	{
		Gtk::TreeModel::iterator itRow = pTreeModel_->append(to);
		(*itRow)[byDirColumns.fileId] = rec.first;
		(*itRow)[byDirColumns.filename] = 
				fs::path(rec.second.header.fileName).filename().string();
		
		
		Gtk::TreeModel::Path path = pTreeModel_->get_path(itRow);
		
		file2record_.insert(std::make_pair(rec.first, 
			Gtk::TreeModel::RowReference(pTreeModel_, std::move(path))));
		
		if (rec.second.header.isDir)
		{
			fillData(rec.first, (*itRow)->children());
			
			if ((*itRow)->children().empty())
			{
				pTreeModel_->erase(itRow);
			}
		}
	}
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
