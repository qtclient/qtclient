#include "client/translationloader.h"

#include "app/bootstrap.h"
#include "shared/tibiafilesystemhelper.h"

#include <QCoreApplication>
#include <QTranslator>

namespace tibia::client {

TTranslationLoader::TTranslationLoader(std::shared_ptr<shared::TTibiaFileSystemHelper> TibiaFileSystemHelper)
    : m_pTibiaFileSystemHelper(std::move(TibiaFileSystemHelper))
{
}

TTranslationLoader::~TTranslationLoader()
{
    if (m_pTranslator)
        QCoreApplication::removeTranslator(m_pTranslator.get());
    m_pTranslator.reset();
}

bool TTranslationLoader::reloadTranslator()
{
    if (m_pTranslator) {
        QCoreApplication::removeTranslator(m_pTranslator.get());
        m_pTranslator.reset();
    }
    m_pTranslator = app::createClientTranslator();
    // Host: the translation file is in qtclient's resources (CIP: the
    // application directory).
    if (!m_pTranslator->load(m_pTibiaFileSystemHelper->getTranslationFileName(),app::clientTranslationDirectory()))
        return false;
    return QCoreApplication::installTranslator(m_pTranslator.get());
}

}
