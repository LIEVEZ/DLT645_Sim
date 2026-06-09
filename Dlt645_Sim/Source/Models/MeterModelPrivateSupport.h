#ifndef METERMODELPRIVATESUPPORT_H
#define METERMODELPRIVATESUPPORT_H

#include "MeterModelPrivateRate.h"

namespace MeterModelPrivate
{
    inline bool isSinglePhaseUnsupportedCode(const QString &diCode)
    {
        static const QSet<QString> unsupportedCodes = {
            QStringLiteral("0x0201FF00"),
            QStringLiteral("0x0202FF00"),
            QStringLiteral("0x0203FF00"),
            QStringLiteral("0x0204FF00"),
            QStringLiteral("0x0205FF00"),
            QStringLiteral("0x0206FF00"),
            QStringLiteral("0x0207FF00"),
            QStringLiteral("0x0208FF00"),
            QStringLiteral("0x0209FF00"),
            QStringLiteral("0x02010200"),
            QStringLiteral("0x02010300"),
            QStringLiteral("0x02020200"),
            QStringLiteral("0x02020300"),
            QStringLiteral("0x02030200"),
            QStringLiteral("0x02030300"),
            QStringLiteral("0x02040200"),
            QStringLiteral("0x02040300"),
            QStringLiteral("0x02050200"),
            QStringLiteral("0x02050300"),
            QStringLiteral("0x02060200"),
            QStringLiteral("0x02060300"),
            QStringLiteral("0x02070200"),
            QStringLiteral("0x02070300"),
            QStringLiteral("0x02080200"),
            QStringLiteral("0x02080300"),
            QStringLiteral("0x02090200"),
            QStringLiteral("0x02090300"),
            QStringLiteral("0x23010002"),
            QStringLiteral("0x23010003"),
            QStringLiteral("0x23010006"),
            QStringLiteral("0x23010007"),
            QStringLiteral("0x230100FF")};
        if (unsupportedCodes.contains(diCode))
        {
            return true;
        }

        int phaseIndex = 0;
        if ((isHarmonicFamilyCode(diCode, QStringLiteral("0x020A"), &phaseIndex) ||
             isHarmonicFamilyCode(diCode, QStringLiteral("0x020B"), &phaseIndex)) &&
            phaseIndex >= 2)
        {
            return true;
        }

        if (isDemandDataCode(diCode))
        {
            bool ok = false;
            const int prefixValue = prefixedCodeValue(diCode, &ok);
            if (!ok)
            {
                return false;
            }

            const int familyValue = prefixValue % 0x1000;
            return (familyValue >= 0x0129 && familyValue <= 0x0132) ||
                   (familyValue >= 0x01A8 && familyValue <= 0x01AE) ||
                   (familyValue >= 0x013D && familyValue <= 0x0146) ||
                   (familyValue >= 0x01BC && familyValue <= 0x01C2);
        }

        if (!isEnergyDataCode(diCode))
        {
            return false;
        }

        bool ok = false;
        const int prefixValue = prefixedCodeValue(diCode, &ok);
        if (!ok)
        {
            return false;
        }

        const int familyValue = energyBaseFamilyFromPrefixValue(prefixValue);
        return (familyValue >= 0x0029 && familyValue <= 0x0032) ||
               (familyValue >= 0x003D && familyValue <= 0x0046) ||
               (familyValue >= 0x00A8 && familyValue <= 0x00AE) ||
               (familyValue >= 0x00BC && familyValue <= 0x00C2);
    }
}

#endif // METERMODELPRIVATESUPPORT_H