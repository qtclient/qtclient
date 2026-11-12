// Stand-in for Quick Controls 1 Settings: CIP LegacyControls QML still reads
// these flags; CipSoft-Tibia/Qt5LegacyControls does not ship the type.
#pragma once

#include <QtCore/qobject.h>
#include <QtQml/qqml.h>

class QQuickControlSettings1 : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(Settings)
  QML_SINGLETON
  Q_PROPERTY(bool isMobile READ isMobile CONSTANT)
  Q_PROPERTY(bool hasTouchScreen READ hasTouchScreen CONSTANT)
  Q_PROPERTY(bool hoverEnabled READ hoverEnabled CONSTANT)
  Q_PROPERTY(int dragThreshold READ dragThreshold CONSTANT)

public:
  using QObject::QObject;

  bool isMobile() const { return false; }
  bool hasTouchScreen() const { return false; }
  bool hoverEnabled() const { return true; }
  int dragThreshold() const { return 10; }
};
