#include <gtest/gtest.h>

#include "assets/asset.h"
#include "assets/assetfilesystemhelper.h"
#include "filesystemhelper.h"
#include "shared/tibiafilesystemhelper.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>
#include <set>

using tibia::assets::TAsset;
using tibia::assets::TAssetFileSystemHelper;
using tibia::shared::TTibiaFileSystemHelper;

namespace {

void writeFile(const QDir& dir, const QString& name, const QByteArray& bytes)
{
    QFile file(dir.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

/// A client file system helper whose assets and sounds directories are `assets` and `sounds`.
std::shared_ptr<TTibiaFileSystemHelper> tibiaFileSystem(const QTemporaryDir& base, const QTemporaryDir& assets, const QTemporaryDir& sounds)
{
    auto helper = std::make_shared<TTibiaFileSystemHelper>(QDir(base.path()), std::make_shared<tibia::shared::TFileSystemHelper>());
    helper->setAssetsDirectories(QDir(assets.path()), QDir(sounds.path()));
    return helper;
}

}

TEST(AssetFileSystemHelper, WriteAndReadAnAssetInItsTypesDirectory)
{
    QTemporaryDir base;
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(tibiaFileSystem(base, assets, sounds));

    const TAsset sprite(TAsset::Sprite, QStringLiteral("sprites.bin"));
    const QString spritePath = helper.writeAsset(sprite, QByteArrayLiteral("pixels"));
    EXPECT_EQ(spritePath, QDir(assets.path()).filePath(QStringLiteral("sprites.bin")));
    EXPECT_EQ(helper.readAsset(sprite), QByteArrayLiteral("pixels"));

    const TAsset sound(TAsset::Sound, QStringLiteral("hit.ogg"));
    EXPECT_EQ(helper.writeAsset(sound, QByteArrayLiteral("noise")), QDir(sounds.path()).filePath(QStringLiteral("hit.ogg")));
    EXPECT_EQ(helper.readAsset(sound), QByteArrayLiteral("noise"));
    EXPECT_EQ(helper.assetsDirectory(), QDir(assets.path()));
    EXPECT_EQ(helper.soundsDirectory(), QDir(sounds.path()));
}

TEST(AssetFileSystemHelper, RemoveAssetDeletesTheFileOfTheAssetsDirectory)
{
    QTemporaryDir base;
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(tibiaFileSystem(base, assets, sounds));
    writeFile(QDir(assets.path()), QStringLiteral("old.dat"), "x");

    helper.removeAsset(TAsset(TAsset::Appearances, QStringLiteral("old.dat")));
    EXPECT_FALSE(QFile::exists(QDir(assets.path()).filePath(QStringLiteral("old.dat"))));
}

TEST(AssetFileSystemHelper, FilesThatAreNotRetainedAreDeleted)
{
    QTemporaryDir base;
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const TAssetFileSystemHelper helper(tibiaFileSystem(base, assets, sounds));
    const QDir dir(assets.path());
    writeFile(dir, QStringLiteral("keep.dat"), "k");
    writeFile(dir, QStringLiteral("drop.dat"), "d");

    std::set<QString> asked;
    helper.forEachLocalFileDecideWhetherItShouldBeRetained([&asked](const QString& name) {
        asked.insert(name);
        return name == QStringLiteral("keep.dat");
    });

    EXPECT_EQ(asked, (std::set<QString>{ QStringLiteral("drop.dat"), QStringLiteral("keep.dat") }));
    EXPECT_TRUE(QFile::exists(dir.filePath(QStringLiteral("keep.dat"))));
    EXPECT_FALSE(QFile::exists(dir.filePath(QStringLiteral("drop.dat"))));
}

TEST(AssetFileSystemHelper, TakesTheDirectoriesOfTheClientFileSystemHelper)
{
    QTemporaryDir base;
    QTemporaryDir assets;
    QTemporaryDir sounds;
    const auto tibiaFileSystemHelper = tibiaFileSystem(base, assets, sounds);
    const TAssetFileSystemHelper helper(tibiaFileSystemHelper);
    EXPECT_EQ(helper.assetsDirectory(), QDir(assets.path()));
    EXPECT_EQ(helper.soundsDirectory(), QDir(sounds.path()));

    // The directories are asked again each time; the paths keep the ones taken at construction.
    QTemporaryDir other;
    tibiaFileSystemHelper->setAssetsDirectories(QDir(other.path()), QDir(other.path()));
    EXPECT_EQ(helper.assetsDirectory(), QDir(other.path()));
    EXPECT_EQ(helper.getFilePath(TAsset::Sprite, QStringLiteral("a.bin")), QDir(assets.path()).filePath(QStringLiteral("a.bin")));
}
