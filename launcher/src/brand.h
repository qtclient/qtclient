#pragma once

#include <QString>
#include <QStringList>

/**
 * qtclient defaults are CipSoft (compile-time). A tip pack under
 * applicationDirPath()/mods/<id>/ with brand.json (+ optional images/launcher)
 * overrides identity and chrome at runtime — same idea as client --no-mods /
 * --default-mod.
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
    /// `--default-mod`: tip mods/default only; Play forwards --default-mod.
    bool defaultMod = false;

    static LauncherBrand& instance();
    /// Parse argv; scan tip mods/ (or QTCLIENT_MODS_DIR) for brand.json unless --no-mods.
    static void load(int argc, char* argv[]);

    /// Prefer pack images/launcher/<file>, else :/images/launcher/<file>.
    QString chromeUrl(const QString& fileName) const;
    QString chromePath(const QString& fileName) const;
};
