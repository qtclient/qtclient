#pragma once

#include <memory>

class QTranslator;

namespace tibia {
namespace shared {
class TTibiaFileSystemHelper;
}
}

namespace tibia {
namespace client {

/// CIP: what the application keeps of its translation: the file system helper
/// that names the translation file and the translator installed from it.
/// The translator leaves the application when the loader goes.
class TTranslationLoader
{
public:
    explicit TTranslationLoader(std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper);
    ~TTranslationLoader();

    TTranslationLoader(const TTranslationLoader&) = delete;
    TTranslationLoader& operator=(const TTranslationLoader&) = delete;

    /// CIP: the installed translator is removed and a new one loads the
    /// helper's translation file (client.en) from the application directory;
    /// it is installed when it loaded. False when it did not load. Host: the
    /// file is in qtclient's resources, and the translator shows the
    /// application name for "Tibia" (app::createClientTranslator).
    bool reloadTranslator();

private:
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_pTibiaFileSystemHelper;
    std::unique_ptr<QTranslator> m_pTranslator;
};

}
}
