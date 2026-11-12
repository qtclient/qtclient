#include <gtest/gtest.h>

#include "qtclient/weather/weathersounds.h"
#include "qtclient/weather/weatherstate.h"
#include "renderer/worldgeometry.h"

#include <QJsonObject>

using tibia::qtclient::WeatherKind;
using tibia::qtclient::WeatherState;
using tibia::qtclient::weatherStateFromOpcode;
using tibia::qmlcomponents::WorldGeometry;

TEST(WeatherState, HasAtmosphereIgnoresBareKind)
{
    WeatherState s;
    s.kind = WeatherKind::Rain;
    s.intensity = 0.f;
    s.fog = 0.f;
    s.clouds = 0.f;
    EXPECT_FALSE(s.hasAtmosphere());

    s.intensity = 0.2f;
    EXPECT_TRUE(s.hasAtmosphere());

    s.intensity = 0.f;
    s.fog = 0.5f;
    EXPECT_TRUE(s.hasAtmosphere());

    s.fog = 0.f;
    s.clouds = 0.1f;
    EXPECT_TRUE(s.hasAtmosphere());
}

TEST(WeatherState, NormalizeClearsKindWhenLayersEmpty)
{
    WeatherState s;
    s.kind = WeatherKind::Storm;
    s.intensity = 0.f;
    s.normalizeLayers();
    EXPECT_EQ(s.kind, WeatherKind::Clear);
    EXPECT_FLOAT_EQ(s.intensity, 0.f);
}

TEST(WeatherOpcode, ZeroLayersBecomeClear)
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("rain"));
    o.insert(QStringLiteral("intensity"), 0);
    o.insert(QStringLiteral("fog"), 0);
    o.insert(QStringLiteral("clouds"), 0);
    const WeatherState s = weatherStateFromOpcode(o);
    EXPECT_EQ(s.kind, WeatherKind::Clear);
    EXPECT_FALSE(s.hasAtmosphere());
}

TEST(WeatherOpcode, LegacyFogKindMapsToFogLayer)
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("fog"));
    o.insert(QStringLiteral("intensity"), 0.4);
    const WeatherState s = weatherStateFromOpcode(o);
    EXPECT_EQ(s.kind, WeatherKind::Clear);
    EXPECT_FLOAT_EQ(s.intensity, 0.f);
    EXPECT_NEAR(s.fog, 0.4f, 1e-5f);
    EXPECT_TRUE(s.hasAtmosphere());
}

TEST(WeatherOpcode, ParsesThunderSoundId)
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"), QStringLiteral("storm"));
    o.insert(QStringLiteral("intensity"), 0.8);
    o.insert(QStringLiteral("thunderSoundId"), 42);
    const WeatherState s = weatherStateFromOpcode(o);
    EXPECT_EQ(s.kind, WeatherKind::Storm);
    EXPECT_EQ(s.thunderSoundId, 42);
    EXPECT_TRUE(s.hasAtmosphere());
}

TEST(WeatherSounds, ResolveBedsOutdoorIndoor)
{
    using namespace tibia::qtclient::weather_sounds;
    WeatherState rain;
    rain.kind = WeatherKind::Rain;
    rain.intensity = 0.3f;
    {
        const BedChoice out = resolvePrecipBed(rain, true);
        EXPECT_EQ(out.shelter, Shelter::Outdoor);
        EXPECT_STREQ(out.path, kRainOutdoorLight);
        EXPECT_EQ(out.fallbackAmbientId, kRainLight);
    }
    rain.intensity = 0.8f;
    {
        const BedChoice out = resolvePrecipBed(rain, true);
        EXPECT_STREQ(out.path, kRainOutdoorHeavy);
        EXPECT_EQ(resolvePrecipBed(rain), kRainHeavy);
    }
    {
        const BedChoice inn = resolvePrecipBed(rain, false);
        EXPECT_EQ(inn.shelter, Shelter::Indoor);
        EXPECT_STREQ(inn.path, kRainIndoorRoof);
    }

    WeatherState storm;
    storm.kind = WeatherKind::Storm;
    storm.intensity = 0.9f;
    {
        const BedChoice out = resolvePrecipBed(storm, true);
        EXPECT_STREQ(out.path, kRainOutdoorHeavy);
        EXPECT_STREQ(resolveThunderClapPath(storm), kThunderClap);
        EXPECT_STREQ(resolveThunderRumblePath(storm), kThunderRumble);
        EXPECT_EQ(resolveThunderEffect(storm), 0);
    }
    {
        const BedChoice inn = resolvePrecipBed(storm, false);
        EXPECT_EQ(inn.shelter, Shelter::Indoor);
        EXPECT_STREQ(inn.path, kRainIndoorRoof);
    }
    EXPECT_STREQ(resolveThunderRumblePath(rain), kThunderRumble);
    EXPECT_EQ(resolveThunderClapPath(rain), nullptr);

    WeatherState snow;
    snow.kind = WeatherKind::Snow;
    snow.intensity = 0.5f;
    EXPECT_EQ(resolvePrecipBed(snow), kSnow);

    WeatherState building;
    building.kind = WeatherKind::Clear;
    building.clouds = 0.7f;
    EXPECT_EQ(resolvePrecipBed(building), kWindFront);

    rain.precipSoundId = 99;
    EXPECT_EQ(resolvePrecipBed(rain), 99);
}

TEST(WorldGeometry, OutdoorAtAndFraction)
{
    WorldGeometry geo;
    EXPECT_FALSE(geo.outdoorAt(0, 0));
    EXPECT_FLOAT_EQ(geo.outdoorFraction(), 0.f);

    geo.width = 2;
    geo.height = 2;
    geo.outdoorMask = QImage(2, 2, QImage::Format_Grayscale8);
    geo.outdoorMask.fill(0);
    geo.outdoorMask.scanLine(0)[0] = 255;
    geo.outdoorMask.scanLine(1)[0] = 255;
    EXPECT_TRUE(geo.outdoorAt(0, 0));
    EXPECT_FALSE(geo.outdoorAt(1, 0));
    EXPECT_TRUE(geo.outdoorAt(0, 1));
    EXPECT_NEAR(geo.outdoorFraction(), 0.5f, 1e-5f);

    geo.outdoorMask.fill(255);
    EXPECT_TRUE(geo.outdoorAt(1, 1));
    EXPECT_FLOAT_EQ(geo.outdoorFraction(), 1.f);

    geo.outdoorMask.fill(0);
    EXPECT_FALSE(geo.outdoorAt(0, 0));
    EXPECT_FLOAT_EQ(geo.outdoorFraction(), 0.f);
}
