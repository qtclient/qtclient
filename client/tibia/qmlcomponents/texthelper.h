#pragma once

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// QML singleton TextHelper (CIP: TTextHelperQmlService, metacall 0x1010886d0).
/// Multi strings join their longest-to-shortest variants with U+009C so QML
/// Text elides to the first one that fits.
class TTextHelperQmlService : public cip::tibia::qmlcomponents::TTextHelperQmlService
{
    Q_OBJECT
public:
    using cip::tibia::qmlcomponents::TTextHelperQmlService::TTextHelperQmlService;

    QString formatNumberWithThousandSeparators(qlonglong Number) override;
    QString formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(qlonglong Number) override;
    QString formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(qlonglong Number, bool WithPlusForPositiveValues) override;
    QString formatTwoNumbersWithThousandSeparatorsAndThousandShortcutsAsMultiString(const QString& FormatString, qlonglong FirstNumber,
                                                                                   qlonglong SecondNumber, bool WithPlusForPositiveValues) override;
    QString formatNumberWithThousandShortcutsAsMultiString(qlonglong Number) override;
    QString formatNumberWithThousandSeparatorsShortestShortcut(qlonglong Number) override;
    QString formatNumberToShortStringWithMax999k(qlonglong Number) override;
    QString formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(int Seconds) override;
    QString formatSecondsToShortHoursMinutesSecondsTimeString(qlonglong Seconds) override;
    QString formatSecondsToUltraShortDaysOrHoursOrMinsOrSecondsTimeString(qlonglong Seconds) override;
    QString formatSecondsToCompactHoursMinutesTimeString(qlonglong Seconds) override;
    QString formatSecondsToMinutesTimeString(qlonglong Seconds) override;
    QString capitalizeEachWordInString(const QString& Text) override;
    QString replaceColorCodingsWithHtmlTags(const QString& TextWithColorCodings) override;
    QString formatDateTime(int Timestamp, bool WithSeconds) override;
    QString formatDateTime(int Timestamp) override;
    QString formatDateTimeWithTimeZoneBerlin(int Timestamp, bool WithSeconds) override;
    QString formatDateTimeWithTimeZoneBerlin(int Timestamp) override;
    QString createQmlMultiString(const QString& LongString, const QString& ShortString) override;
};

}
