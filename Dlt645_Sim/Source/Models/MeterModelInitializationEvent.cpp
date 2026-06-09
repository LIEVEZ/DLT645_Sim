#include "MeterModelInitializationEventPrivate.h"

using namespace MeterModelEventPrivate;

void MeterModel::initializeEventData()
{
    const EventInitializationContext context;
    initializePhaseEventData(context, m_dataItems);
    initializeStatisticalEventData(context, m_dataItems);
    initializeProgrammingEventData(context, m_dataItems, m_operatorCode);
}