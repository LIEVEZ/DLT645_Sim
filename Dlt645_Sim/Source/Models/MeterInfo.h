#ifndef METERINFO_H
#define METERINFO_H

#include <QString>

#include "Models/MeterPhaseType.h"

struct MeterInfo
{
    QString alias;
    QString address;
    QString operatorCode;
    QString password;
    bool includeLeadingFE = false;
    MeterPhaseType phaseType = MeterPhaseType::ThreePhase;
    bool eventRecordPresent = true;
};

#endif // METERINFO_H