#include "texthelper.h"

#include "chat/chatchannel.h"

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>

namespace tibia::qmlcomponents {

namespace {

const QChar MultiStringSeparator(0x9c);

QString grouped(const qlonglong number)
{
    return QLocale(QLocale::English).toString(number);
}

/// Grouped, then " k"/"k", " kk"/"kk", … per further factor of 1,000 the
/// magnitude reaches (up to "kkkkkk" at 10^18).
QStringList shortcutVariants(const qlonglong number, const bool withPlus)
{
    const QString sign = withPlus && number > 0 ? QStringLiteral("+") : QString();
    QStringList variants{ sign + grouped(number) };
    QString suffix;
    for (qlonglong divisor = 1000; divisor <= 1000000000000000000LL; divisor *= 1000) {
        if (number < divisor && number > -divisor)
            break;
        suffix += u'k';
        const QString value = sign + grouped(number / divisor);
        variants << value + u' ' + suffix << value + suffix;
        if (divisor == 1000000000000000000LL)
            break;
    }
    return variants;
}

}

QString shortDurationString(const qint64 seconds)
{
    if (seconds < 3601) {
        if (seconds < 60)
            return qtTrId("time_second_short").arg(seconds);
        if (seconds % 60 == 0)
            return qtTrId("time_minutes_short").arg(seconds / 60);
        return qtTrId("time_minutes_and_seconds_short").arg(seconds / 60).arg(seconds % 60);
    }
    const qint64 minutes = seconds / 60;
    if (minutes < 1441) {
        if (minutes < 60)
            return qtTrId("time_minutes_short").arg(minutes % 60);
        return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
    }
    return qtTrId("time_days_hours_and_minutes_short").arg(minutes / 1440).arg((minutes % 1440) / 60).arg(minutes % 60);
}

QString ultraShortDurationString(const qint64 seconds)
{
    const QChar zero(u'0');
    if (seconds >= 864000)
        return QStringLiteral("%1d").arg(seconds / 86400);
    // The day form's second part is the minutes into the day, not the hours.
    if (seconds >= 86400)
        return QStringLiteral("%1d%2").arg(seconds / 86400).arg((seconds % 86400) / 60, 2, 10, zero);
    if (seconds >= 36000)
        return QStringLiteral("%1h").arg(seconds / 3600);
    if (seconds >= 3600)
        return QStringLiteral("%1h%2").arg(seconds / 3600).arg((seconds % 3600) / 60, 2, 10, zero);
    if (seconds >= 600)
        return QStringLiteral("%1m").arg(seconds / 60);
    if (seconds >= 60)
        return QStringLiteral("%1m%2").arg(seconds / 60).arg(seconds % 60, 2, 10, zero);
    return QStringLiteral("%1s").arg(seconds);
}

QString hoursMinutesSecondsString(const qint64 seconds)
{
    const QString hoursMinutes = QStringLiteral("%1:%2").arg(seconds / 3600, 2, 10, QChar(u'0')).arg((seconds / 60) % 60, 2, 10, QChar(u'0'));
    return QStringLiteral("%1:%2").arg(hoursMinutes).arg(seconds % 60, 2, 10, QChar(u'0'));
}

QString thousandShortcutsMultiString(const qlonglong number, const bool withPlus)
{
    return shortcutVariants(number, withPlus).join(MultiStringSeparator);
}

QString TTextHelperQmlService::formatNumberWithThousandSeparators(const qlonglong Number)
{
    return grouped(Number);
}

QString TTextHelperQmlService::formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(const qlonglong Number)
{
    return thousandShortcutsMultiString(Number, false);
}

QString TTextHelperQmlService::formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(const qlonglong Number,
                                                                                                  const bool WithPlusForPositiveValues)
{
    return thousandShortcutsMultiString(Number, WithPlusForPositiveValues);
}

/// Every pairing of the two numbers' variants.
QString TTextHelperQmlService::formatTwoNumbersWithThousandSeparatorsAndThousandShortcutsAsMultiString(
    const QString& FormatString, const qlonglong FirstNumber, const qlonglong SecondNumber, const bool WithPlusForPositiveValues)
{
    const QStringList first = shortcutVariants(FirstNumber, WithPlusForPositiveValues);
    const QStringList second = shortcutVariants(SecondNumber, WithPlusForPositiveValues);
    QStringList variants;
    for (const QString& a : first) {
        for (const QString& b : second)
            variants << FormatString.arg(a, b);
    }
    return variants.join(MultiStringSeparator);
}

QString TTextHelperQmlService::formatNumberWithThousandShortcutsAsMultiString(const qlonglong Number)
{
    QStringList variants{ grouped(Number) };
    const QString plain = QString::number(Number);
    if (plain != variants.first())
        variants << plain;
    if (Number > 999)
        variants << QString::number(Number / 1000) + u'k';
    if (Number > 999999)
        variants << QString::number(Number / 1000000) + QStringLiteral("kk");
    return variants.join(MultiStringSeparator);
}

QString TTextHelperQmlService::formatNumberWithThousandSeparatorsShortestShortcut(const qlonglong Number)
{
    if (Number >= 1000000 || Number <= -1000000)
        return grouped(Number / 1000000) + QStringLiteral(" kk");
    if (Number < 10000 && Number > -10000)
        return grouped(Number);
    return grouped(Number / 1000) + QStringLiteral(" k");
}

QString TTextHelperQmlService::formatNumberToShortStringWithMax999k(const qlonglong Number)
{
    const auto value = static_cast<uint>(Number);
    if (value >= 1000000)
        return QStringLiteral("999k+");
    if (value >= 100000)
        return QStringLiteral("%1k").arg(value / 1000);
    return QString::number(value);
}

/// Up to an hour (inclusive) minutes and seconds, then hours and minutes, days from a day on.
QString TTextHelperQmlService::formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(const int Seconds)
{
    return shortDurationString(Seconds);
}

/// "HH:MM:SS".
QString TTextHelperQmlService::formatSecondsToShortHoursMinutesSecondsTimeString(const qlonglong Seconds)
{
    return hoursMinutesSecondsString(Seconds);
}

QString TTextHelperQmlService::formatSecondsToUltraShortDaysOrHoursOrMinsOrSecondsTimeString(const qlonglong Seconds)
{
    return ultraShortDurationString(Seconds);
}

/// On whole minutes.
QString TTextHelperQmlService::formatSecondsToCompactHoursMinutesTimeString(const qlonglong Seconds)
{
    const qlonglong minutes = Seconds / 60;
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    if (minutes % 60 < 1)
        return qtTrId("time_hour_short").arg(minutes / 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

QString TTextHelperQmlService::formatSecondsToMinutesTimeString(const qlonglong Seconds)
{
    if (Seconds < 60)
        return qtTrId("time_less_than_one_minute");
    return qtTrId("time_minutes").arg(QString::number((Seconds + 30) / 60));
}

QString TTextHelperQmlService::capitalizeEachWordInString(const QString& Text)
{
    if (Text.isEmpty())
        return {};
    QStringList words = Text.split(u' ', Qt::SkipEmptyParts);
    for (QString& word : words)
        word = word.first(1).toUpper() + word.mid(1);
    return words.join(u' ');
}

/// "{#rrggbb|text}" -> font tags.
QString TTextHelperQmlService::replaceColorCodingsWithHtmlTags(const QString& TextWithColorCodings)
{
    return chat::replaceColorCodingsWithHtmlTags(TextWithColorCodings);
}

QString TTextHelperQmlService::formatDateTime(const int Timestamp, const bool WithSeconds)
{
    QString format = QStringLiteral("yyyy-MM-dd, HH:mm");
    if (WithSeconds)
        format += QStringLiteral(":ss");
    return QDateTime::fromSecsSinceEpoch(Timestamp).toString(format);
}

QString TTextHelperQmlService::formatDateTime(const int Timestamp)
{
    return formatDateTime(Timestamp, true);
}

QString TTextHelperQmlService::formatDateTimeWithTimeZoneBerlin(const int Timestamp, const bool WithSeconds)
{
    QString format = QStringLiteral("yyyy-MM-dd, HH:mm");
    if (WithSeconds)
        format += QStringLiteral(":ss");
#if QT_CONFIG(timezone)
    const QDateTime berlin = QDateTime::fromSecsSinceEpoch(Timestamp).toTimeZone(QTimeZone("Europe/Berlin"));
    const bool summer = berlin.isDaylightTime();
#else
    // Qt for WebAssembly has no tz database: EU rule, CEST from 01:00 UTC on the last Sunday of
    // March to 01:00 UTC on the last Sunday of October.
    const QDateTime utc = QDateTime::fromSecsSinceEpoch(Timestamp, QTimeZone::UTC);
    const int year = utc.date().year();
    const auto switchAt = [year](const int month) {
        const QDate last(year, month, QDate(year, month, 1).daysInMonth());
        return QDateTime(last.addDays(-(last.dayOfWeek() % 7)), QTime(1, 0), QTimeZone::UTC);
    };
    const bool summer = utc >= switchAt(3) && utc < switchAt(10);
    const QDateTime berlin = utc.toOffsetFromUtc(summer ? 7200 : 3600);
#endif
    return qtTrId("%1 %2").arg(berlin.toString(format), summer ? QStringLiteral("CEST") : QStringLiteral("CET"));
}

QString TTextHelperQmlService::formatDateTimeWithTimeZoneBerlin(const int Timestamp)
{
    return formatDateTimeWithTimeZoneBerlin(Timestamp, false);
}

QString TTextHelperQmlService::createQmlMultiString(const QString& LongString, const QString& ShortString)
{
    return QStringList{ LongString, ShortString }.join(MultiStringSeparator);
}

QString TTextHelperQmlService::wrapTextWithHtmlFontColor(const QString& HtmlText, const QColor& Color)
{
    QString text = HtmlText;
    text.insert(0, QStringLiteral("<font color=\"%1\">").arg(Color.name()));
    text.append(QStringLiteral("</font>"));
    return text;
}

}
