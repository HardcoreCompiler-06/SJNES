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

#pragma once
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>

// AviWriter (giữ tên cũ để không phải sửa SJNES.h) - nay xuất thẳng ra MP4 (H.264 + AAC)
// thông qua ffmpeg:
//   - video: đẩy frame BGR24 (top-down) vào stdin của ffmpeg, mã hóa H.264 ra file tạm
//   - audio: ghi PCM16 ra file WAV tạm
//   - stop(): ghép video + audio thành file .mp4 cuối cùng, xóa file tạm
// Cần ffmpeg.exe nằm cạnh file .exe của SJNES hoặc nằm trong PATH.
class AviWriter {
public:
    AviWriter() = default;
    ~AviWriter() { if (m_open) stop(); }

    // path: file .mp4 đầu ra
    // width/height: kích thước khung hình NES (256x240)
    // fps: tốc độ video đầu ra (frame được lấy theo thời gian thực rồi chuẩn hóa về fps này)
    // sampleRate/channels: định dạng PCM audio thật đang phát
    // Trả về false (và đặt lastError()) nếu không tìm thấy/không chạy được ffmpeg.
    bool start(const QString& path, int width, int height, int fps, int sampleRate, int channels = 1);

    // bgrBuffer: BGR24, top-down (không lật dòng), size = width*height*3
    void writeVideoFrame(const uint8_t* bgrBuffer, size_t sizeBytes);

    // pcm: PCM 16-bit signed, interleaved nếu channels>1
    void writeAudioSamples(const int16_t* pcm, size_t sampleCount);

    // Dừng quay. KHÔNG chặn UI: việc mã hóa nốt + ghép video/audio chạy nền (qua signal của
    // QProcess). isRecording() trả về false ngay; khi file .mp4 sẵn sàng thì gọi callback
    // đặt bằng setOnFinished() (chạy trên UI thread).
    void stop();

    // cb(ok, path): ok=false nếu ffmpeg lỗi; path là file .mp4 đầu ra.
    void setOnFinished(std::function<void(bool, const QString&)> cb) { m_onFinished = std::move(cb); }

    bool isRecording() const { return m_open; }
    QString lastError() const { return m_error; }

private:
    static QString findFfmpeg();
    void writeWavHeader();
    void patchWavHeader();

    bool m_open = false;
    int m_width = 0, m_height = 0, m_fps = 60;
    int m_sampleRate = 44100, m_channels = 1;

    QString m_ffmpegPath;
    QString m_outPath;
    QString m_tmpVideo;
    QString m_tmpAudio;
    QString m_error;

    std::unique_ptr<QProcess> m_ffmpeg; // encoder video (nhận frame qua stdin)
    QFile m_wav;
    uint32_t m_audioBytes = 0;

    // Đồng hồ thực để ép video về đúng fps cố định (lặp/bỏ frame theo thời điểm frame đến)
    QElapsedTimer m_clock;
    qint64 m_videoFrames = 0;

    std::function<void(bool, const QString&)> m_onFinished;
};