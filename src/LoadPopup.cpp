#include "LoadPopup.hpp"
#include "BotMenu.hpp"
#include "MacroManager.hpp"

using namespace geode::prelude;

namespace macrobot {

LoadPopup* LoadPopup::create() {
    auto ret = new LoadPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LoadPopup::init() {
    if (!Popup::init(260.f, 220.f)) return false;

    this->setTitle("Load Macro");

    auto winSize = m_mainLayer->getContentSize();
    auto& mgr = MacroManager::get();

    m_files.clear();
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(mgr.macrosDir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".mbf") {
            m_files.push_back(entry.path());
        }
    }

    if (m_files.empty()) {
        auto label = CCLabelBMFont::create("No saved macros yet", "chatFont.fnt");
        label->setScale(0.6f);
        label->setPosition({winSize.width * 0.5f, winSize.height * 0.5f});
        m_mainLayer->addChild(label);
        return true;
    }

    // Simple scrolling list of one button per saved macro file.
    auto listSize = CCSize{winSize.width - 30.f, winSize.height - 60.f};
    auto scroll = ScrollLayer::create(listSize);
    scroll->setPosition({(winSize.width - listSize.width) / 2.f, 30.f});
    m_mainLayer->addChild(scroll);

    float rowHeight = 30.f;
    float rowGap = 10.f;
    float rowStep = rowHeight + rowGap; // gap so entries aren't stuck together
    float y = static_cast<float>(m_files.size()) * rowStep;

    for (size_t i = 0; i < m_files.size(); i++) {
        auto name = m_files[i].stem().string();
        auto btnSprite = ButtonSprite::create(name.c_str(), "bigFont.fnt", "GJ_button_01.png", 0.7f);
        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(LoadPopup::onPick)
        );
        btn->setTag(static_cast<int>(i));

        auto rowMenu = CCMenu::create();
        rowMenu->setContentSize({listSize.width, rowHeight});
        rowMenu->addChild(btn);
        btn->setPosition({listSize.width * 0.5f, rowHeight * 0.5f});
        rowMenu->setPosition({0.f, y - rowStep});
        scroll->m_contentLayer->addChild(rowMenu);

        y -= rowStep;
    }

    scroll->m_contentLayer->setContentSize({listSize.width, static_cast<float>(m_files.size()) * rowStep});
    scroll->moveToTop();

    return true;
}

void LoadPopup::onPick(CCObject* sender) {
    auto item = static_cast<CCMenuItemSpriteExtra*>(sender);
    size_t idx = static_cast<size_t>(item->getTag());
    if (idx >= m_files.size()) return;

    auto& mgr = MacroManager::get();
    if (!mgr.loadMacroFromFile(m_files[idx])) {
        FLAlertLayer::create("MacroBot", "Failed to load that macro file.", "OK")->show();
        return;
    }

    // Spec: picking a macro here should get back to the bot menu and
    // auto-click Play. We skip straight to playback and close both
    // popups, which is equivalent (and faster) for the player.
    beginPlayback();
    this->keyBackClicked();
    BotMenuPopup::closeIfOpen();
}

}
