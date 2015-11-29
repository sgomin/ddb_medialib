#include "main_widget.hpp"

#include "settings_dlg.hpp"

struct ByDirectoryColumns : Gtk::TreeModel::ColumnRecord
{
	Gtk::TreeModelColumn<Glib::ustring>	filename;
	ByDirectoryColumns() { add(filename); }
};

static const ByDirectoryColumns byDirColumns;

MainWidget::MainWidget(Database & db)
 : db_(db)
{
    Gtk::HBox * pFirstRow = Gtk::manage(new Gtk::HBox());
	sidebar_.pack_start(*pFirstRow, Gtk::PACK_SHRINK);
    
	Gtk::ComboBoxText * pStyleCombo = Gtk::manage(new Gtk::ComboBoxText());
	pFirstRow->pack_start(*pStyleCombo, Gtk::PACK_EXPAND_WIDGET);
	pStyleCombo->append("By directory structure");
    pStyleCombo->set_active(0);
	
	Gtk::Button* pSettingsBtn = Gtk::manage(new Gtk::Button("Settings"));
	pFirstRow->pack_end(*pSettingsBtn, Gtk::PACK_SHRINK);
	pSettingsBtn->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWidget::onSettings));
    
    Gtk::ScrolledWindow * pScrolledWindow = Gtk::manage(new Gtk::ScrolledWindow());
	sidebar_.pack_start(*pScrolledWindow, Gtk::PACK_EXPAND_WIDGET);
	pScrolledWindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	
    Gtk::TreeView * pTreeVeiew = Gtk::manage(new Gtk::TreeView());
    pScrolledWindow->add(*pTreeVeiew);
    
	pTreeModel_ = Gtk::TreeStore::create(byDirColumns);
	fillData(ROOT_RECORD_ID, pTreeModel_->children());
	pTreeVeiew->set_model(pTreeModel_);
	pTreeVeiew->append_column("File Name", byDirColumns.filename);
    
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
		Gtk::TreeStore::iterator itRow = pTreeModel_->append(to);
		(*itRow)[byDirColumns.filename] = rec.second.header.fileName;
		
		if (rec.second.header.isDir)
		{
			fillData(rec.first, itRow->children());
		}
	}
}
