#ifndef METERPHASETYPE_H
#define METERPHASETYPE_H

#include <QString>

enum class MeterPhaseType
{
    SinglePhase,
    ThreePhase
};

inline QString meterPhaseTypeToDisplayName(MeterPhaseType meterPhaseType)
{
    return meterPhaseType == MeterPhaseType::SinglePhase
               ? QStringLiteral("单相表")
               : QStringLiteral("三相表");
}

#endif // METERPHASETYPE_H
