import QtQuick

// Outlined text; the official client caches it in a texture.
Text {
  enum CacheMode { ReleaseLater, ReleaseImmediately, NoCaching }
  property int cacheMode: CachedOutlineText.ReleaseLater
  property bool debugOverlay: false
  property bool highQuality: false
  style: Text.Outline
}
