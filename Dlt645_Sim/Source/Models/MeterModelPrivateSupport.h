#ifndef METERMODELPRIVATESUPPORT_H
#define METERMODELPRIVATESUPPORT_H

#include "MeterModelPrivateRate.h"

namespace MeterModelPrivate
{
    inline bool isSinglePhaseUnsupportedCode(const QString &diCode)
    {
        static const QSet<QString> unsupportedCodes = {
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
            QStringLiteral("0x23010007")};
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

        // 事件记录（0x03XX）：单相表只支持 A 相事件
        if (diCode.startsWith(QStringLiteral("0x03")))
        {
            // 失压/欠压/过压事件：0x030100/0x030200/0x030300
            // - 总次数：0x03010000(A相) 0x03010001(B相) 0x03010002(C相)
            // - 历史记录：0x030101XX(A相) 0x030102XX(B相) 0x030103XX(C相)
            if (diCode.startsWith(QStringLiteral("0x0301")) ||
                diCode.startsWith(QStringLiteral("0x0302")) ||
                diCode.startsWith(QStringLiteral("0x0303")))
            {
                // 总次数：格式为 0x030X0000、0x030X0001、0x030X0002，倒数第三、第四字节为 "00"
                // 单相表过滤 B相(01) 和 C相(02)
                if (diCode.length() == 10)
                {
                    const QString thirdFourthByte = diCode.mid(4, 2);
                    if (thirdFourthByte == QStringLiteral("00"))
                    {
                        // 这是总次数格式
                        if (diCode.endsWith(QStringLiteral("01")) || diCode.endsWith(QStringLiteral("02")))
                        {
                            return true;
                        }
                    }
                    else
                    {
                        // 这是历史记录格式：0x030XPPNN (PP=相别 01/02/03, NN=历史序号)
                        // 倒数第二个字节(mid(6,2))为相别（01=A, 02=B, 03=C）
                        const QString phaseCode = diCode.mid(6, 2);
                        if (phaseCode == QStringLiteral("02") || phaseCode == QStringLiteral("03"))
                        {
                            return true;
                        }
                    }
                }
            }

            // 失流/过流/断流/潮流反向/过载事件：0x030B00~0x030F00
            // - 总次数：0x030B0000(A相) 0x030B0001(B相) 0x030B0002(C相)
            // - 历史记录：0x030B01XX(A相) 0x030B02XX(B相) 0x030B03XX(C相)
            if (diCode.startsWith(QStringLiteral("0x030B")) ||
                diCode.startsWith(QStringLiteral("0x030C")) ||
                diCode.startsWith(QStringLiteral("0x030D")) ||
                diCode.startsWith(QStringLiteral("0x030E")) ||
                diCode.startsWith(QStringLiteral("0x030F")))
            {
                if (diCode.length() == 10)
                {
                    const QString thirdFourthByte = diCode.mid(4, 2);
                    if (thirdFourthByte == QStringLiteral("00"))
                    {
                        // 这是总次数格式
                        if (diCode.endsWith(QStringLiteral("01")) || diCode.endsWith(QStringLiteral("02")))
                        {
                            return true;
                        }
                    }
                    else
                    {
                        // 这是历史记录格式
                        const QString phaseCode = diCode.mid(6, 2);
                        if (phaseCode == QStringLiteral("02") || phaseCode == QStringLiteral("03"))
                        {
                            return true;
                        }
                    }
                }
            }

            // 断相/相序事件也类似处理（如果有）
            if (diCode.startsWith(QStringLiteral("0x0304")))
            {
                if (diCode.length() == 10)
                {
                    const QString thirdFourthByte = diCode.mid(4, 2);
                    if (thirdFourthByte == QStringLiteral("00"))
                    {
                        // 这是总次数格式
                        if (diCode.endsWith(QStringLiteral("01")) || diCode.endsWith(QStringLiteral("02")))
                        {
                            return true;
                        }
                    }
                    else
                    {
                        // 这是历史记录格式
                        const QString phaseCode = diCode.mid(6, 2);
                        if (phaseCode == QStringLiteral("02") || phaseCode == QStringLiteral("03"))
                        {
                            return true;
                        }
                    }
                }
            }

            // 电压合格率统计：0x0310
            // - 总统计：0x03100000(本月) 0x031000XX(上X月)，第三字节为 "00"
            // - 分相统计：0x031001XX(A相) 0x031002XX(B相) 0x031003XX(C相)
            //   第三字节(mid(6,2))直接表示相别（00=总, 01=A, 02=B, 03=C）
            if (diCode.startsWith(QStringLiteral("0x0310")))
            {
                if (diCode.length() == 10)
                {
                    const QString phaseCode = diCode.mid(6, 2);
                    if (phaseCode == QStringLiteral("02") || phaseCode == QStringLiteral("03"))
                    {
                        return true;
                    }
                }
            }
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
