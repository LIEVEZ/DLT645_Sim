#include "MeterModelPrivate.h"

using namespace MeterModelPrivate;

void MeterModel::initializeEnergyData()
{
    // 电能量数据 (0x00xxxxxx)

    // 1. 组合有功电能 [0000]
    m_dataItems["0x00000000"] = DataItem("0x00000000", "(当前)组合有功总电能", 1.2, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000100"] = DataItem("0x00000100", "(当前)组合有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000200"] = DataItem("0x00000200", "(当前)组合有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000300"] = DataItem("0x00000300", "(当前)组合有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000400"] = DataItem("0x00000400", "(当前)组合有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00000500"] = DataItem("0x00000500", "(当前)组合有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0000FF00"] = DataItem("0x0000FF00", "(当前)组合有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);
    // 2. 正向有功电能 [0001]
    m_dataItems["0x00010000"] = DataItem("0x00010000", "(当前)正向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010100"] = DataItem("0x00010100", "(当前)正向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010200"] = DataItem("0x00010200", "(当前)正向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010300"] = DataItem("0x00010300", "(当前)正向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010400"] = DataItem("0x00010400", "(当前)正向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00010500"] = DataItem("0x00010500", "(当前)正向有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0001FF00"] = DataItem("0x0001FF00", "(当前)正向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 3. 反向有功电能 [0002]
    m_dataItems["0x00020000"] = DataItem("0x00020000", "(当前)反向有功总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020100"] = DataItem("0x00020100", "(当前)反向有功费率1电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020200"] = DataItem("0x00020200", "(当前)反向有功费率2电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020300"] = DataItem("0x00020300", "(当前)反向有功费率3电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020400"] = DataItem("0x00020400", "(当前)反向有功费率4电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00020500"] = DataItem("0x00020500", "(当前)反向有功费率5电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0002FF00"] = DataItem("0x0002FF00", "(当前)反向有功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 4. 组合无功 1 电能 [0003]
    m_dataItems["0x00030000"] = DataItem("0x00030000", "(当前)组合无功1总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030100"] = DataItem("0x00030100", "(当前)组合无功1费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030200"] = DataItem("0x00030200", "(当前)组合无功1费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030300"] = DataItem("0x00030300", "(当前)组合无功1费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030400"] = DataItem("0x00030400", "(当前)组合无功1费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00030500"] = DataItem("0x00030500", "(当前)组合无功1费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0003FF00"] = DataItem("0x0003FF00", "(当前)组合无功1电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 5. 组合无功 2 电能 [0004]
    m_dataItems["0x00040000"] = DataItem("0x00040000", "(当前)组合无功2总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040100"] = DataItem("0x00040100", "(当前)组合无功2费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040200"] = DataItem("0x00040200", "(当前)组合无功2费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040300"] = DataItem("0x00040300", "(当前)组合无功2费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040400"] = DataItem("0x00040400", "(当前)组合无功2费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00040500"] = DataItem("0x00040500", "(当前)组合无功2费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0004FF00"] = DataItem("0x0004FF00", "(当前)组合无功2电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 6. 四象限无功电能 [0005-0008]
    // 第一象限无功电能 [0005]
    m_dataItems["0x00050000"] = DataItem("0x00050000", "(当前)第一象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050100"] = DataItem("0x00050100", "(当前)第一象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050200"] = DataItem("0x00050200", "(当前)第一象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050300"] = DataItem("0x00050300", "(当前)第一象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050400"] = DataItem("0x00050400", "(当前)第一象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00050500"] = DataItem("0x00050500", "(当前)第一象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0005FF00"] = DataItem("0x0005FF00", "(当前)第一象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第二象限无功电能 [0006]
    m_dataItems["0x00060000"] = DataItem("0x00060000", "(当前)第二象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060100"] = DataItem("0x00060100", "(当前)第二象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060200"] = DataItem("0x00060200", "(当前)第二象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060300"] = DataItem("0x00060300", "(当前)第二象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060400"] = DataItem("0x00060400", "(当前)第二象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00060500"] = DataItem("0x00060500", "(当前)第二象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0006FF00"] = DataItem("0x0006FF00", "(当前)第二象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第三象限无功电能 [0007]
    m_dataItems["0x00070000"] = DataItem("0x00070000", "(当前)第三象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070100"] = DataItem("0x00070100", "(当前)第三象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070200"] = DataItem("0x00070200", "(当前)第三象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070300"] = DataItem("0x00070300", "(当前)第三象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070400"] = DataItem("0x00070400", "(当前)第三象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00070500"] = DataItem("0x00070500", "(当前)第三象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0007FF00"] = DataItem("0x0007FF00", "(当前)第三象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 第四象限无功电能 [0008]
    m_dataItems["0x00080000"] = DataItem("0x00080000", "(当前)第四象限无功总电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080100"] = DataItem("0x00080100", "(当前)第四象限无功费率1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080200"] = DataItem("0x00080200", "(当前)第四象限无功费率2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080300"] = DataItem("0x00080300", "(当前)第四象限无功费率3电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080400"] = DataItem("0x00080400", "(当前)第四象限无功费率4电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00080500"] = DataItem("0x00080500", "(当前)第四象限无功费率5电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0008FF00"] = DataItem("0x0008FF00", "(当前)第四象限无功电能数据块", QVariantList(), "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 7. 视在电能 [0009-000A]
    // 正向视在电能 [0009]
    m_dataItems["0x00090000"] = DataItem("0x00090000", "(当前)正向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090100"] = DataItem("0x00090100", "(当前)正向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090200"] = DataItem("0x00090200", "(当前)正向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090300"] = DataItem("0x00090300", "(当前)正向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090400"] = DataItem("0x00090400", "(当前)正向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00090500"] = DataItem("0x00090500", "(当前)正向视在费率5电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x0009FF00"] = DataItem("0x0009FF00", "(当前)正向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 反向视在电能 [000A]
    m_dataItems["0x000A0000"] = DataItem("0x000A0000", "(当前)反向视在总电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0100"] = DataItem("0x000A0100", "(当前)反向视在费率1电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0200"] = DataItem("0x000A0200", "(当前)反向视在费率2电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0300"] = DataItem("0x000A0300", "(当前)反向视在费率3电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0400"] = DataItem("0x000A0400", "(当前)反向视在费率4电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000A0500"] = DataItem("0x000A0500", "(当前)反向视在费率5电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x000AFF00"] = DataItem("0x000AFF00", "(当前)反向视在电能数据块", 0.0, "-", "F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA:F9,AA,AA,.,AA", false, 2);

    // 8. 其他电能数据 [0080-0086]
    m_dataItems["0x00800000"] = DataItem("0x00800000", "(当前)关联总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00810000"] = DataItem("0x00810000", "(当前)正向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00820000"] = DataItem("0x00820000", "(当前)反向有功基波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00830000"] = DataItem("0x00830000", "(当前)正向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00840000"] = DataItem("0x00840000", "(当前)反向有功谐波总电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00850000"] = DataItem("0x00850000", "(当前)铜损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00860000"] = DataItem("0x00860000", "(当前)铁损有功总电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 9. A 相电能数据 [0015-001E, 0094-009A]
    m_dataItems["0x00150000"] = DataItem("0x00150000", "(当前)A相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00160000"] = DataItem("0x00160000", "(当前)A相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00170000"] = DataItem("0x00170000", "(当前)A相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00180000"] = DataItem("0x00180000", "(当前)A相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00190000"] = DataItem("0x00190000", "(当前)A相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001A0000"] = DataItem("0x001A0000", "(当前)A相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001B0000"] = DataItem("0x001B0000", "(当前)A相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001C0000"] = DataItem("0x001C0000", "(当前)A相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001D0000"] = DataItem("0x001D0000", "(当前)A相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x001E0000"] = DataItem("0x001E0000", "(当前)A相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00940000"] = DataItem("0x00940000", "(当前)A相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00950000"] = DataItem("0x00950000", "(当前)A相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00960000"] = DataItem("0x00960000", "(当前)A相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00970000"] = DataItem("0x00970000", "(当前)A相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00980000"] = DataItem("0x00980000", "(当前)A相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00990000"] = DataItem("0x00990000", "(当前)A相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x009A0000"] = DataItem("0x009A0000", "(当前)A相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 10. B 相电能数据 [0029-0032, 00A8-00AE]
    m_dataItems["0x00290000"] = DataItem("0x00290000", "(当前)B相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002A0000"] = DataItem("0x002A0000", "(当前)B相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002B0000"] = DataItem("0x002B0000", "(当前)B相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002C0000"] = DataItem("0x002C0000", "(当前)B相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002D0000"] = DataItem("0x002D0000", "(当前)B相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002E0000"] = DataItem("0x002E0000", "(当前)B相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x002F0000"] = DataItem("0x002F0000", "(当前)B相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00300000"] = DataItem("0x00300000", "(当前)B相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00310000"] = DataItem("0x00310000", "(当前)B相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00320000"] = DataItem("0x00320000", "(当前)B相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00A80000"] = DataItem("0x00A80000", "(当前)B相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00A90000"] = DataItem("0x00A90000", "(当前)B相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AA0000"] = DataItem("0x00AA0000", "(当前)B相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AB0000"] = DataItem("0x00AB0000", "(当前)B相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AC0000"] = DataItem("0x00AC0000", "(当前)B相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AD0000"] = DataItem("0x00AD0000", "(当前)B相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00AE0000"] = DataItem("0x00AE0000", "(当前)B相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    // 11. C 相电能数据 [003D-0046, 00BC-00C2]
    m_dataItems["0x003D0000"] = DataItem("0x003D0000", "(当前)C相正向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x003E0000"] = DataItem("0x003E0000", "(当前)C相反向有功电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x003F0000"] = DataItem("0x003F0000", "(当前)C相组合无功1电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00400000"] = DataItem("0x00400000", "(当前)C相组合无功2电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00410000"] = DataItem("0x00410000", "(当前)C相第一象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00420000"] = DataItem("0x00420000", "(当前)C相第二象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00430000"] = DataItem("0x00430000", "(当前)C相第三象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00440000"] = DataItem("0x00440000", "(当前)C相第四象限无功电能", 0.0, "kvarh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00450000"] = DataItem("0x00450000", "(当前)C相正向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00460000"] = DataItem("0x00460000", "(当前)C相反向视在电能", 0.0, "kVAh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BC0000"] = DataItem("0x00BC0000", "(当前)C相关联电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BD0000"] = DataItem("0x00BD0000", "(当前)C相正向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BE0000"] = DataItem("0x00BE0000", "(当前)C相反向有功基波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00BF0000"] = DataItem("0x00BF0000", "(当前)C相正向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C00000"] = DataItem("0x00C00000", "(当前)C相反向有功谐波电能", 0.0, "kWh", "F9,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C10000"] = DataItem("0x00C10000", "(当前)C相铜损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);
    m_dataItems["0x00C20000"] = DataItem("0x00C20000", "(当前)C相铁损有功电能补偿量", 0.0, "kWh", "AA,AA,AA,.,AA", false, 2);

    initializeSettlementDayEnergyData(m_dataItems);
    synchronizeCurrentEnergyTotals(m_dataItems);
}

void MeterModel::initializeDemandData()
{
    // 最大需量及发生时间 (0x01xxxxxx)
    const QDateTime now = QDateTime::currentDateTime();

    int familyIndex = 0;
    for (const DemandFamilyDefinition &family : currentDemandFamilies())
    {
        initializeCurrentDemandFamily(m_dataItems, family, now, familyIndex);
        ++familyIndex;
    }
    initializeSettlementDayDemandData(m_dataItems);
    synchronizeDemandTotals(m_dataItems);
}

