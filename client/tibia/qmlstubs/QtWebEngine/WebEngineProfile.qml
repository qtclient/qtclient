import QtQml

QtObject {
    enum HttpCacheType { MemoryHttpCache, DiskHttpCache, NoCache }
    enum PersistentCookiesPolicy { NoPersistentCookies, AllowPersistentCookies, ForcePersistentCookies }

    property int httpCacheType: WebEngineProfile.MemoryHttpCache
    property int persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
}
