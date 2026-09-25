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
        auto label = CCLabelBMFont::create(
            "No saved macros yet",
            "chatFont.fnt"
        );

        label->setScale(0.6f);
        label->setPosition({
            winSize.width * 0.5f,
            winSize.height * 0.5f
        });

        m_mainLayer->addChild(label);
        return true;
    }

    // Simple scrolling list of one button per saved macro file.
    auto listSize = CCSize{
        winSize.width - 30.f,
        winSize.height - 60.f
    };

    auto scroll = ScrollLayer::create(listSize);

    scroll->setPosition({
        (winSize.width - listSize.width) / 2.f,
        30.f
    });

    m_mainLayer->addChild(scroll);

    float rowHeight = 30.f;
    float rowGap = 10.f;
    float rowStep = rowHeight + rowGap;
    float y = static_cast<float>(m_files.size()) * rowStep;

    for (size_t i = 0; i < m_files.size(); i++) {
        auto name = m_files[i].stem().string();

        auto btnSprite = ButtonSprite::create(
            name.c_str(),
            "bigFont.fnt",
            "GJ_button_01.png",
            0.7f
        );

        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(LoadPopup::onPick)
        );

        btn->setTag(static_cast<int>(i));

        auto rowMenu = CCMenu::create();
        rowMenu->setContentSize({
            listSize.width,
            rowHeight
        });

        rowMenu->addChild(btn);

        btn->setPosition({
            listSize.width * 0.5f,
            rowHeight * 0.5f
        });

        rowMenu->setPosition({
            0.f,
            y - rowStep
        });

        scroll->m_contentLayer->addChild(rowMenu);

        y -= rowStep;
    }

    scroll->m_contentLayer->setContentSize({
        listSize.width,
        static_cast<float>(m_files.size()) * rowStep
    });

    scroll->moveToTop();

    return true;
}

// Converts a macro filename / level name into the same sanitized form
// MacroManager uses when creating filenames.
//
// A final " (number)" suffix is ignored. This means:
//
//   Stereo Madness
//   Stereo Madness (2)
//   Stereo Madness (3)
//
// are all considered to belong to the same level for the compatibility
// warning.
static std::string normalizedLevelFileName(std::string name) {
    if (name.size() > 4 && name.back() == ')') {
        auto open = name.rfind(" (");

        if (open != std::string::npos && open + 3 < name.size()) {
            bool allDigits = true;

            for (size_t i = open + 2; i + 1 < name.size(); ++i) {
                if (!std::isdigit(
                        static_cast<unsigned char>(name[i])
                    )) {
                    allDigits = false;
                    break;
                }
            }

            if (allDigits) {
                name.erase(open);
            }
        }
    }

    // MacroManager sanitizes level names before using them as filenames.
    std::string sanitized;
    sanitized.reserve(name.size());

    for (char c : name) {
        if (
            std::isalnum(static_cast<unsigned char>(c)) ||
            c == ' ' ||
            c == '-' ||
            c == '_'
        ) {
            sanitized.push_back(c);
        } else {
            sanitized.push_back('_');
        }
    }

    return sanitized.empty() ? "macro" : sanitized;
}

void LoadPopup::onPick(CCObject* sender) {
    auto item = static_cast<CCMenuItemSpriteExtra*>(sender);
    size_t idx = static_cast<size_t>(item->getTag());

    if (idx >= m_files.size()) return;

    auto& mgr = MacroManager::get();
    auto path = m_files[idx];

    // Get the current level's actual name.
    std::string currentLevel;

    if (auto pl = PlayLayer::get()) {
        if (pl->m_level) {
            currentLevel = pl->m_level->m_levelName;
        }
    }

    // Compare the macro filename against the current level.
    //
    // The saved filename is used rather than the internal MacroData
    // levelName because the user's requested behavior is specifically
    // based on names such as:
    //
    //     Level
    //     Level (2)
    //     Level (3)
    //
    // where the numeric suffix only means another macro was saved for
    // that same level.
    auto macroName = path.stem().string();

    bool levelMatches =
        !currentLevel.empty() &&
        normalizedLevelFileName(macroName) ==
            normalizedLevelFileName(currentLevel);

    // Loading the macro itself is kept in one callback so that both the
    // normal path and the confirmation path behave identically after
    // the user has made their choice.
    auto loadAndPlay = [this, path]() {
        auto& mgr = MacroManager::get();

        if (!mgr.loadMacroFromFile(path)) {
            FLAlertLayer::create(
                "MacroBot",
                "Failed to load that macro file.",
                "OK"
            )->show();

            return;
        }

        // Picking a macro loads it and immediately starts playback.
        beginPlayback();

        this->keyBackClicked();
        BotMenuPopup::closeIfOpen();
    };

    // If we don't know the current level name, don't block loading.
    //
    // Otherwise, a mismatch requires explicit confirmation before the
    // loaded macro replaces the currently armed macro.
    if (!levelMatches && !currentLevel.empty()) {
        geode::createQuickPopup(
            "MacroBot",
            "Are you sure you want to replace the current recorded inputs?",
            "Cancel",
            "Yes",
            [loadAndPlay](auto, bool confirmed) {
                if (confirmed) {
                    loadAndPlay();
                }
            }
        );

        return;
    }

    loadAndPlay();
}

}
