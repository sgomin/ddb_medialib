#include "settings_dlg.hpp"

#include <vector>


struct DirListColumns : Gtk::TreeModel::ColumnRecord
{
    Gtk::TreeModelColumn<Glib::ustring> path;
    DirListColumns() { add(path); }
};

static const DirListColumns dirListColumns; // TODO: make non-static


SettingsDlg::SettingsDlg(Settings & settings) :
    Gtk::Dialog("Settings", /*modal*/ true),
    settings_(settings),
    btnDel_(Gtk::Stock::DELETE)
{
    initDirList();
  
    for (auto dir : settings_.directories)
    {
        addDirectory(dir.first, dir.second);
    }
    
    Gtk::HBox * pMainBox = Gtk::manage(new Gtk::HBox());
    Gtk::Frame * pFrame = Gtk::manage(new Gtk::Frame("Media Directories"));
    Gtk::VBox * pRightBox = Gtk::manage(new Gtk::VBox());
    
    Gtk::Button * pBtnAdd = Gtk::manage(new Gtk::Button(Gtk::Stock::ADD));
    pBtnAdd->signal_clicked().connect(sigc::mem_fun(*this, &SettingsDlg::onAddDir));
    btnDel_.signal_clicked().connect(sigc::mem_fun(*this, &SettingsDlg::onDelDir));
    
    //    btnDel_.set_sensitive(false);
    pFrame->add(dirList_);
    pRightBox->pack_end(btnDel_, Gtk::PACK_SHRINK);
    pRightBox->pack_end(*pBtnAdd, Gtk::PACK_SHRINK);
    
    pMainBox->pack_start(*pFrame, Gtk::PACK_EXPAND_WIDGET, 1);
    pMainBox->pack_start(*pRightBox, Gtk::PACK_SHRINK);

 #ifdef USE_GTK2
    get_vbox()->pack_start(*pMainBox, Gtk::PACK_EXPAND_WIDGET, 1);
 #else
    get_content_area()->set_orientation(Gtk::ORIENTATION_VERTICAL);
    get_content_area()->pack_start(*pMainBox, Gtk::PACK_EXPAND_WIDGET, 1);
 #endif
    
    this->add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    this->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    
    show_all();
}


void SettingsDlg::initDirList()
{
    pListModel_ = Gtk::ListStore::create(dirListColumns);
    dirList_.set_model(pListModel_);
    dirList_.append_column("Path", dirListColumns.path);
    dirList_.set_headers_visible(false);
    //    dirList_.signal_selection_notify_event().connect(
    //        sigc::mem_fun(*this, &SettingsDlg::onSelNotify));
}


void SettingsDlg::onAddDir()
{
    Gtk::FileChooserDialog dlg(
        "Choose media directory", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    dlg.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dlg.add_button(Gtk::Stock::ADD, Gtk::RESPONSE_OK);
    dlg.set_select_multiple(true);
    
    if (dlg.run() == Gtk::RESPONSE_OK)
    {
        std::vector<std::string> dirnames = dlg.get_filenames();
        
        for (std::string& dirname : dirnames)
        {
			Settings::Directory dirSettings;
			dirSettings.recursive = true;
			auto inserted = settings_.directories.insert(
				std::make_pair(std::move(dirname), std::move(dirSettings)));
			
            if (inserted.second)
            {
                addDirectory(inserted.first->first, inserted.first->second);
            }
        }
        
        show_all();
    }
}
    
void SettingsDlg::onDelDir()
{
    auto selection = dirList_.get_selection();
    auto itRow = selection->get_selected();
    
    if (itRow)
    {
        Glib::ustring dir = (*itRow)[ dirListColumns.path ];
        settings_.directories.erase(dir);
        pListModel_->erase(itRow);
        selection->unselect_all();
	//btnDel_.set_sensitive(false);
        show_all();
    }
}


void SettingsDlg::addDirectory(
	const std::string & dirname, 
    const Settings::Directory & /*settings*/)
{
    auto itNewRow = pListModel_->append();
    (*itNewRow)[ dirListColumns.path ] = dirname;
}
