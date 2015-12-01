#include "main_widget.hpp"

#include "settings_dlg.hpp"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

struct ByDirectoryColumns : Gtk::TreeModel::ColumnRecord
{
	Gtk::TreeModelColumn<Glib::ustring>	fullPath;
	Gtk::TreeModelColumn<Glib::ustring>	filename;
	ByDirectoryColumns() { add(fullPath); add(filename); }
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
		(*itRow)[byDirColumns.fullPath] = rec.second.header.fileName;
		(*itRow)[byDirColumns.filename] = 
				fs::path(rec.second.header.fileName).filename().string();
		
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
