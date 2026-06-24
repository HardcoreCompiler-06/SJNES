#include "NSFFile.h"
#include <QFile>
#include <cstring>

uint16_t NSFFile::ReadLE16(const QByteArray& data, int offset)
{
    uint8_t lo = static_cast<uint8_t>(data[offset]);
    uint8_t hi = static_cast<uint8_t>(data[offset + 1]);
    return static_cast<uint16_t>(lo | (hi << 8));
}

QString NSFFile::ReadText32(const QByteArray& data, int offset)
{
    QByteArray text = data.mid(offset, 32);

    int zeroIndex = text.indexOf('\0');
    if (zeroIndex >= 0)
        text = text.left(zeroIndex);

    return QString::fromLatin1(text).trimmed();
}

bool NSFFile::LoadFromFile(const QString& path, QString* error)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        if (error) *error = "Không mở được file NSF.";
        return false;
    }

    QByteArray data = file.readAll();

    if (data.size() < 0x80)
    {
        if (error) *error = "File NSF quá nhỏ.";
        return false;
    }

    if (!(data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 'M' && static_cast<uint8_t>(data[4]) == 0x1A))
    {
        if (error) *error = "Sai header NSF. File không phải NSF hợp lệ.";
        return false;
    }

    info = NSFInfo{};
    programData.clear();

    info.version = static_cast<uint8_t>(data[0x05]);
    info.totalSongs = static_cast<uint8_t>(data[0x06]);
    info.startingSong = static_cast<uint8_t>(data[0x07]);

    info.loadAddress = ReadLE16(data, 0x08);
    info.initAddress = ReadLE16(data, 0x0A);
    info.playAddress = ReadLE16(data, 0x0C);

    info.songName = ReadText32(data, 0x0E);
    info.artist = ReadText32(data, 0x2E);
    info.copyright = ReadText32(data, 0x4E);

    info.ntscSpeed = ReadLE16(data, 0x6E);

    for (int i = 0; i < 8; i++)
        info.bankSwitch[i] = static_cast<uint8_t>(data[0x70 + i]);

    info.palSpeed = ReadLE16(data, 0x78);
    info.region = static_cast<uint8_t>(data[0x7A]);
    info.expansion = static_cast<uint8_t>(data[0x7B]);

    const int programOffset = 0x80;
    programData.assign(
        reinterpret_cast<const uint8_t*>(data.constData() + programOffset),
        reinterpret_cast<const uint8_t*>(data.constData() + data.size())
    );

    if (programData.empty())
    {
        if (error) *error = "NSF không có program data.";
        return false;
    }

    return true;
}

bool NSFFile::UsesBankSwitching() const
{
    for (int i = 0; i < 8; i++)
    {
        if (info.bankSwitch[i] != 0)
            return true;
    }

    return false;
}

bool NSFFile::UsesVRC6() const
{
    return (info.expansion & 0x01) != 0;
}

bool NSFFile::UsesVRC7() const
{
    return (info.expansion & 0x02) != 0;
}

bool NSFFile::UsesFDS() const
{
    return (info.expansion & 0x04) != 0;
}

bool NSFFile::UsesMMC5() const
{
    return (info.expansion & 0x08) != 0;
}

bool NSFFile::UsesN163() const
{
    return (info.expansion & 0x10) != 0;
}

bool NSFFile::UsesS5B() const
{
    return (info.expansion & 0x20) != 0;
}