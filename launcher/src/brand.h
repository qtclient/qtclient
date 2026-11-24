#pragma once

#include <QString>
#include <QStringList>

/**
 * qtclient defaults are CipSoft (compile-time). A tip pack under
 * applicationDirPath()/mods/<id>/ with brand.json (+ optional images/launcher)
 * overrides identity and chrome at runtime — same idea as client --no-mods.
 *
 * Non-CipSoft download channels of a build are enabled by the tip packs'
 * launcher.json "channels" (see LauncherMods); stock CipSoft has none.
 */
struct LauncherBrand {
    QString id;
    QString productName = QStringLiteral(TIBIA_PRODUCT_NAME);
    QString productOrg = QStringLiteral(TIBIA_PRODUCT_ORG);
    QString productUrl = QStringLiteral(TIBIA_PRODUCT_URL);
    QString defaultCdn = QStringLiteral(TIBIA_DEFAULT_CDN);
    QString clientPackageDir = QStringLiteral(TIBIA_CLIENT_PACKAGE_DIR);
    QStringList clientPackageVariants;
    QString packRoot; // empty = stock qtclient (qrc chrome)
    bool loaded = false;
    /// `--no-mods`: stock CIP chrome; Play forwards --no-mods to client.
    bool noMods = false;

    static LauncherBrand& instance();
    /// Parse argv; scan tip mods/ (or QTCLIENT_MODS_DIR) for brand.json unless --no-mods.
    static void load(int argc, char* argv[]);

    /// Prefer pack images/launcher/<file>, else tip CDN URL, else :/images/launcher/<file>.
    QString chromeUrl(const QString& fileName) const;
    QString chromePath(const QString& fileName) const;
    /// Tip CDN URL for launcher chrome (`{defaultCdn|/QTCLIENT_CDN_URL}/images/launcher/<file>`).
    QString chromeCdnUrl(const QString& fileName) const;
    /// Path usable in stylesheets / QPixmap: pack image if present, else the qrc one.
    /// (QSS cannot load remote URLs — CDN splash is for network Image / /play.)
    QString imagePath(const QString& fileName) const;
};

/// Tip launcher config: every tip mods/<id>/launcher.json lists the download
/// channels it enables (`"channels": ["<name>", …]`). A build of the launcher
/// configuration publishes a channel as `<name>url` / `<name>versionurl`.
struct LauncherMods {
    QStringList channels;

    static LauncherMods& instance();
    /// Scan tip mods/<id>/launcher.json unless --no-mods.
    static void load(bool noMods);
};
