/*
 * SJNES - Nintendo Entertainment System Emulator
 * Copyright (C) 2026 Chiến
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "AviWriter.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

// ---- helper ghi little-endian ----
static void writeU32(QFile& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
static void writeU16(QFile& f, uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); }

QString AviWriter::findFfmpeg()
{
    // 1) ffmpeg.exe cạnh file .exe của SJNES (dễ đóng gói cùng app nhất)
    const QString local = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (QFileInfo::exists(local))
        return local;
    // 2) ffmpeg trong PATH
    return QStandardPaths::findExecutable("ffmpeg");
}

void AviWriter::writeWavHeader()
{
    QFile& f = m_wav;
    f.write("RIFF", 4);
    writeU32(f, 0);                       // RIFF size - patch sau
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    writeU32(f, 16);
    writeU16(f, 1);                       // PCM
    writeU16(f, (uint16_t)m_channels);
    writeU32(f, (uint32_t)m_sampleRate);
    writeU32(f, (uint32_t)(m_sampleRate * m_channels * 2)); // byte rate
    writeU16(f, (uint16_t)(m_channels * 2));                // block align
    writeU16(f, 16);                      // bits per sample
    f.write("data", 4);
    writeU32(f, 0);                       // data size - patch sau
}

void AviWriter::patchWavHeader()
{
    m_wav.seek(4);
    writeU32(m_wav, 36 + m_audioBytes);
    m_wav.seek(40);
    writeU32(m_wav, m_audioBytes);
}

bool AviWriter::start(const QString& path, int width, int height, int fps, int sampleRate, int channels)
{
    if (m_open) stop();
    m_error.clear();

    m_ffmpegPath = findFfmpeg();
    if (m_ffmpegPath.isEmpty()) {
        m_error = "Không tìm thấy ffmpeg. Hãy đặt ffmpeg.exe cạnh file SJNES.exe "
            "hoặc thêm ffmpeg vào PATH.";
        return false;
    }

    m_width = width; m_height = height; m_fps = fps;
    m_sampleRate = sampleRate; m_channels = channels;
    m_outPath = path;
    m_tmpVideo = path + ".video.tmp.mp4";
    m_tmpAudio = path + ".audio.tmp.wav";
    m_audioBytes = 0;

    m_wav.setFileName(m_tmpAudio);
    if (!m_wav.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = "Không tạo được file tạm âm thanh: " + m_tmpAudio;
        return false;
    }
    writeWavHeader();

    // Video: raw BGR24 vào stdin, fps cố định (-framerate). Việc khớp thời gian thực do
    // writeVideoFrame() tự làm (lặp/bỏ frame theo đồng hồ lúc frame đến), không dựa vào
    // thời điểm ffmpeg đọc pipe (bị dồn cục nên gây giật).
    // scale nearest-neighbor x3 giữ pixel art sắc nét khi xem/đăng lên mạng.
    m_ffmpeg = std::make_unique<QProcess>();
    QStringList args;
    args << "-y" << "-hide_banner" << "-loglevel" << "error"
        << "-f" << "rawvideo"
        << "-pix_fmt" << "bgr24"
        << "-s" << QString("%1x%2").arg(m_width).arg(m_height)
        << "-framerate" << QString::number(m_fps)
        << "-i" << "-"
        << "-c:v" << "libx264" << "-preset" << "veryfast" << "-crf" << "18"
        << "-pix_fmt" << "yuv420p"
        << m_tmpVideo;
    m_ffmpeg->start(m_ffmpegPath, args);

    if (!m_ffmpeg->waitForStarted(5000)) {
        m_error = "Không chạy được ffmpeg: " + m_ffmpeg->errorString();
        m_wav.close();
        QFile::remove(m_tmpAudio);
        m_ffmpeg.reset();
        return false;
    }

    m_videoFrames = 0;
    m_clock.start();
    m_open = true;
    return true;
}

void AviWriter::writeVideoFrame(const uint8_t* bgrBuffer, size_t sizeBytes)
{
    if (!m_open || !m_ffmpeg || m_ffmpeg->state() != QProcess::Running) return;
    if (sizeBytes != static_cast<size_t>(m_width) * m_height * 3) return;
    const qint64 target = (m_clock.nsecsElapsed() * m_fps + 500000000LL) / 1000000000LL;
    while (m_videoFrames <= target) {
        m_ffmpeg->write(reinterpret_cast<const char*>(bgrBuffer), static_cast<qint64>(sizeBytes));
        ++m_videoFrames;
    }
}

void AviWriter::writeAudioSamples(const int16_t* pcm, size_t sampleCount)
{
    if (!m_open) return;
    const uint32_t bytes = static_cast<uint32_t>(sampleCount * sizeof(int16_t));
    m_wav.write(reinterpret_cast<const char*>(pcm), bytes);
    m_audioBytes += bytes;
}

void AviWriter::stop()
{
    if (!m_open) return;
    m_open = false;

    patchWavHeader();
    m_wav.close();
    const QString ffmpegPath = m_ffmpegPath;
    const QString outPath = m_outPath;
    const QString tmpVideo = m_tmpVideo;
    const QString tmpAudio = m_tmpAudio;
    const bool hasAudio = m_audioBytes > 0;
    const auto onFinished = m_onFinished;

    auto cleanup = [tmpVideo, tmpAudio]() {
        QFile::remove(tmpVideo);
        QFile::remove(tmpAudio);
        };
    auto report = [onFinished, outPath](bool ok) {
        if (onFinished) onFinished(ok, outPath);
        };

    QProcess* enc = m_ffmpeg.release();
    if (!enc) {
        cleanup();
        report(false);
        return;
    }

    QObject::connect(enc, &QProcess::finished, enc,
        [=](int exitCode, QProcess::ExitStatus status) {
            const bool encOk = (status == QProcess::NormalExit && exitCode == 0);
            enc->deleteLater();
            if (!encOk) {
                cleanup();
                report(false);
                return;
            }

            QFile::remove(outPath);
            if (!hasAudio) {
                const bool ok = QFile::copy(tmpVideo, outPath);
                cleanup();
                report(ok);
                return;
            }

            QProcess* mux = new QProcess;
            QObject::connect(mux, &QProcess::finished, mux,
                [=](int c, QProcess::ExitStatus s) {
                    const bool ok = (s == QProcess::NormalExit && c == 0);
                    mux->deleteLater();
                    cleanup();
                    report(ok);
                });
            QObject::connect(mux, &QProcess::errorOccurred, mux,
                [=](QProcess::ProcessError e) {
                    if (e == QProcess::FailedToStart) {
                        mux->deleteLater();
                        cleanup();
                        report(false);
                    }
                });
            mux->start(ffmpegPath, QStringList()
                << "-y" << "-hide_banner" << "-loglevel" << "error"
                << "-i" << tmpVideo
                << "-i" << tmpAudio
                << "-c:v" << "copy"
                << "-c:a" << "aac" << "-b:a" << "192k"
                << "-movflags" << "+faststart"
                << outPath);
        });

    enc->closeWriteChannel();
}