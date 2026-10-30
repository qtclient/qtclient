#include "texthelper.h"

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

/// CIP 0x100ce8af0: grouped, then " k"/"k", " kk"/"kk", " kkk"/"kkk" from 1,000 on.
QStringList shortcutVariants(const qlonglong number, const bool withPlus)
{
    const QString sign = withPlus && number > 0 ? QStringLiteral("+") : QString();
    QStringList variants{ sign + grouped(number) };
    const auto add = [&](const qlonglong divisor, const char* suffix) {
        const QString value = sign + grouped(number / divisor);
        variants << value + u' ' + QLatin1String(suffix) << value + QLatin1String(suffix);
    };
    if (number >= 1000 || number <= -1000) {
        add(1000, "k");
        if (number >= 1000000 || number <= -1000000) {
            add(1000000, "kk");
            if (number >= 1000000000 || number <= -1000000000)
                add(1000000000, "kkk");
        }
    }
    return variants;
}

}

/// CIP 0x1010c3220.
QString TTextHelperQmlService::formatNumberWithThousandSeparators(const qlonglong Number)
{
    return grouped(Number);
}

/// CIP 0x100ce8a20.
QString TTextHelperQmlService::formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(const qlonglong Number)
{
    return shortcutVariants(Number, false).join(MultiStringSeparator);
}

QString TTextHelperQmlService::formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(const qlonglong Number,
                                                                                                  const bool WithPlusForPositiveValues)
{
    return shortcutVariants(Number, WithPlusForPositiveValues).join(MultiStringSeparator);
}

/// CIP 0x1010c3ee0: every pairing of the two numbers' variants.
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

/// CIP 0x1010c33e0.
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

/// CIP 0x1010c38d0.
QString TTextHelperQmlService::formatNumberWithThousandSeparatorsShortestShortcut(const qlonglong Number)
{
    if (Number >= 1000000 || Number <= -1000000)
        return grouped(Number / 1000000) + QStringLiteral(" kk");
    if (Number < 10000 && Number > -10000)
        return grouped(Number);
    return grouped(Number / 1000) + QStringLiteral(" k");
}

/// CIP 0x100960b40.
QString TTextHelperQmlService::formatNumberToShortStringWithMax999k(const qlonglong Number)
{
    const auto value = static_cast<uint>(Number);
    if (value >= 1000000)
        return QStringLiteral("999k+");
    if (value >= 100000)
        return QStringLiteral("%1k").arg(value / 1000);
    return QString::number(value);
}

/// CIP 0x10095f080 (below an hour 0x10095ee80, else 0x10095e750/0x10095e350).
QString TTextHelperQmlService::formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(const int Seconds)
{
    const qlonglong s = Seconds;
    if (s < 3600) {
        if (s < 60)
            return qtTrId("time_second_short").arg(s);
        if (s % 60 == 0)
            return qtTrId("time_minutes_short").arg(s / 60);
        return qtTrId("time_minutes_and_seconds_short").arg(s / 60).arg(s % 60);
    }
    const qlonglong minutes = s / 60;
    if (minutes <= 1440)
        return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
    return qtTrId("time_days_hours_and_minutes_short").arg(minutes / 1440).arg((minutes % 1440) / 60).arg(minutes % 60);
}

/// CIP 0x10095eb00: "HH:MM:SS".
QString TTextHelperQmlService::formatSecondsToShortHoursMinutesSecondsTimeString(const qlonglong Seconds)
{
    const QString hoursMinutes = QStringLiteral("%1:%2").arg(Seconds / 3600, 2, 10, QChar(u'0')).arg((Seconds / 60) % 60, 2, 10, QChar(u'0'));
    return QStringLiteral("%1:%2").arg(hoursMinutes).arg(Seconds % 60, 2, 10, QChar(u'0'));
}

/// CIP 0x10095f0d0.
QString TTextHelperQmlService::formatSecondsToUltraShortDaysOrHoursOrMinsOrSecondsTimeString(const qlonglong Seconds)
{
    const QChar zero(u'0');
    if (Seconds >= 864000)
        return QStringLiteral("%1d").arg(Seconds / 86400);
    if (Seconds >= 86400)
        return QStringLiteral("%1d%2").arg(Seconds / 86400).arg((Seconds % 86400) / 3600, 2, 10, zero);
    if (Seconds >= 36000)
        return QStringLiteral("%1h").arg(Seconds / 3600);
    if (Seconds >= 3600)
        return QStringLiteral("%1h%2").arg(Seconds / 3600).arg((Seconds % 3600) / 60, 2, 10, zero);
    if (Seconds >= 600)
        return QStringLiteral("%1m").arg(Seconds / 60);
    if (Seconds >= 60)
        return QStringLiteral("%1m%2").arg(Seconds / 60).arg(Seconds % 60, 2, 10, zero);
    return QStringLiteral("%1s").arg(Seconds);
}

/// CIP 0x10095e540 (on minutes).
QString TTextHelperQmlService::formatSecondsToCompactHoursMinutesTimeString(const qlonglong Seconds)
{
    const qlonglong minutes = Seconds / 60;
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    if (minutes % 60 < 1)
        return qtTrId("time_hour_short").arg(minutes / 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

/// CIP 0x10095f560.
QString TTextHelperQmlService::formatSecondsToMinutesTimeString(const qlonglong Seconds)
{
    if (Seconds < 60)
        return qtTrId("time_less_than_one_minute");
    return qtTrId("time_minutes").arg(QString::number((Seconds + 30) / 60));
}

/// CIP 0x101018f90.
QString TTextHelperQmlService::capitalizeEachWordInString(const QString& Text)
{
    if (Text.isEmpty())
        return {};
    QStringList words = Text.split(u' ', Qt::SkipEmptyParts);
    for (QString& word : words)
        word = word.first(1).toUpper() + word.mid(1);
    return words.join(u' ');
}

/// CIP 0x10099c4e0 / 0x10099cab0: "{#rrggbb|text}" -> font tags.
QString TTextHelperQmlService::replaceColorCodingsWithHtmlTags(const QString& TextWithColorCodings)
{
    static const QRegularExpression coding(QStringLiteral("\\{(#[0-9a-fA-F]{6})\\|([^\\}]*)\\}"));
    QString text = TextWithColorCodings;
    qsizetype from = 0;
    for (QRegularExpressionMatch match = coding.match(text, from); match.hasMatch(); match = coding.match(text, from)) {
        const QString html = QStringLiteral("<font color=\"%1\">%2</font>")
                                 .arg(QColor::fromString(match.captured(1)).name(), match.captured(2));
        text.replace(match.capturedStart(), match.capturedLength(), html);
        from = match.capturedStart() + html.size();
    }
    return text;
}

/// CIP 0x10095c790.
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

/// CIP 0x10095d320.
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
    return QStringLiteral("%1 %2").arg(berlin.toString(format), summer ? QStringLiteral("CEST") : QStringLiteral("CET"));
}

QString TTextHelperQmlService::formatDateTimeWithTimeZoneBerlin(const int Timestamp)
{
    return formatDateTimeWithTimeZoneBerlin(Timestamp, false);
}

/// CIP 0x1010c3cd0.
QString TTextHelperQmlService::createQmlMultiString(const QString& LongString, const QString& ShortString)
{
    return QStringList{ LongString, ShortString }.join(MultiStringSeparator);
}

}
