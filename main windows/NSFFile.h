#pragma once

#include <QString>
#include <QByteArray>
#include <vector>
#include <cstdint>

struct NSFInfo
{
    uint8_t version = 0;
    uint8_t totalSongs = 0;
    uint8_t startingSong = 0;

    uint16_t loadAddress = 0;
    uint16_t initAddress = 0;
    uint16_t playAddress = 0;

    QString songName;
    QString artist;
    QString copyright;

    uint16_t ntscSpeed = 0;
    uint16_t palSpeed = 0;

    uint8_t region = 0;
    uint8_t expansion = 0;

    uint8_t bankSwitch[8] = {};
};

class NSFFile
{
public:
    bool LoadFromFile(const QString& path, QString* error = nullptr);

    const NSFInfo& Info() const { return info; }
    const std::vector<uint8_t>& ProgramData() const { return programData; }

    bool UsesBankSwitching() const;
    bool UsesVRC6() const;
    bool UsesVRC7() const;
    bool UsesFDS() const;
    bool UsesMMC5() const;
    bool UsesN163() const;
    bool UsesS5B() const;

private:
    static uint16_t ReadLE16(const QByteArray& data, int offset);
    static QString ReadText32(const QByteArray& data, int offset);

private:
    NSFInfo info;
    std::vector<uint8_t> programData;
};