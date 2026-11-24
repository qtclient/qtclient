#include "shared/dynamicresourceshelper.h"

#include <QCoreApplication>
#include <QDir>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {

QCoreApplication* application()
{
    static int argc = 1;
    static char name[] = "dynamicresourceshelper_test";
    static char* argv[] = { name, nullptr };
    // Never destroyed: a static application torn down at exit crashes after Qt's statics.
    static QCoreApplication* app = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
    (void)app;
    return static_cast<QCoreApplication*>(QCoreApplication::instance());
}

}

using tibia::shared::TDynamicResourcesHelper;

TEST(DynamicResourcesHelper, ResourceFileLivesNextToTheExecutable)
{
    application();
    const QString expected = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("graphics_resources.rcc"));
    EXPECT_EQ(TDynamicResourcesHelper::getResourceFilePath(QStringLiteral("graphics")), expected);
}

TEST(DynamicResourcesHelper, MissingResourceFileThrowsWithNameAndPath)
{
    application();
    const QString name = QStringLiteral("missing_for_test");
    try {
        TDynamicResourcesHelper::registerResources({ name });
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
        const QString expected = QStringLiteral("Could not register resource '%1'. File '%2' not found")
                                     .arg(name)
                                     .arg(TDynamicResourcesHelper::getResourceFilePath(name));
        EXPECT_EQ(QString::fromStdString(error.what()), expected);
    }
}

TEST(DynamicResourcesHelper, NoNamesRegistersNothing)
{
    application();
    EXPECT_NO_THROW(TDynamicResourcesHelper::registerResources({}));
}
