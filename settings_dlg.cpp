#include "settings_dlg.hpp"

#include <vector>

SettingsDlg::SettingsDlg(Settings & settings) :
    Gtk::Dialog("Settings", /*modal*/ true),
    settings_(settings),
    btnDel_("Delete")
{
    for (auto dir : settings_.directories)
    {
        addDirectory(dir.first, dir.second);
    }
    
    Gtk::HBox * pMainBox = Gtk::manage(new Gtk::HBox());
    Gtk::Frame * pFrame = Gtk::manage(new Gtk::Frame("Media Directories"));
    Gtk::VBox * pRightBox = Gtk::manage(new Gtk::VBox());
    
    Gtk::Button * pBtnAdd = Gtk::manage(new Gtk::Button("Add"));
    
    pBtnAdd->signal_clicked().connect(sigc::mem_fun(*this, &SettingsDlg::onAddDir));
    btnDel_.signal_clicked().connect(sigc::mem_fun(*this, &SettingsDlg::onDelDir));
    lbDirectories_.signal_row_selected().connect(
        sigc::mem_fun(*this, &SettingsDlg::onSelChanged));
    
    btnDel_.set_sensitive(false);
    pFrame->add(lbDirectories_);
    pRightBox->pack_end(btnDel_, Gtk::PACK_SHRINK);
    pRightBox->pack_end(*pBtnAdd, Gtk::PACK_SHRINK);
    
    pMainBox->pack_start(*pFrame, Gtk::PACK_EXPAND_WIDGET, 1);
    pMainBox->pack_start(*pRightBox, Gtk::PACK_SHRINK);
    
    get_content_area()->set_orientation(Gtk::ORIENTATION_VERTICAL);
    get_content_area()->pack_start(*pMainBox, Gtk::PACK_EXPAND_WIDGET, 1);
    
    this->add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    this->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    
    show_all();
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
    Gtk::ListBoxRow*  pRow = lbDirectories_.get_selected_row();
    
    if (pRow)
    {
        std::string dir = static_cast<Gtk::Label*>(pRow->get_child())->get_text();
        settings_.directories.erase(dir);
        pRow->remove();
        lbDirectories_.unselect_row();
        show_all();
    }
    
}

void SettingsDlg::onSelChanged(Gtk::ListBoxRow * pRow)
{
    btnDel_.set_sensitive(pRow != nullptr);
}

void SettingsDlg::addDirectory(
	const std::string & dirname, 
    const Settings::Directory & /*settings*/)
{
    Gtk::Label * pLabel = Gtk::manage(new Gtk::Label(dirname));
    pLabel->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    lbDirectories_.append(*pLabel);
}