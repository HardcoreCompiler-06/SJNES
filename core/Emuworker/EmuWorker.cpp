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

#include "EmuWorker.h"
#include "LogBuffer.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#include "Mapper_085.h"
#include "Mapper_069.h"
#include "Mapper_024.h"
#include "Mapper_020.h"
#include "Cartridge.h"
#include "miniz.h"
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <cmath>
#include <QDebug>


// ---- Lưu đĩa FDS (game ghi đĩa -> patch IPS cạnh file game, đuôi .fdssave) ----
// Đuôi riêng (không dùng .ips) để không đụng file .ips của emulator khác: patch của mình
// tính trên các side nối liền nhau, không có header 16 byte của .fds.
static QString FdsSavePathFor(const QString& fileName)
{
    QFileInfo fi(fileName);
    return fi.absolutePath() + "/" + fi.completeBaseName() + ".fdssave";
}

static std::vector<uint8_t> LoadNesFromZip_Worker(const QString& zipPath)
{
    std::vector<uint8_t> result;

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    QByteArray pathBytes = zipPath.toLocal8Bit();

    if (!mz_zip_reader_init_file(&zip, pathBytes.constData(), 0))
        return result;

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < fileCount; i++)
    {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        QString fileName = QString::fromUtf8(stat.m_filename);
        if (!fileName.endsWith(".nes", Qt::CaseInsensitive))
            continue;

        size_t uncompSize = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, i, &uncompSize, 0);

        if (data && uncompSize > 0)
        {
            uint8_t* bytes = (uint8_t*)data;
            result.assign(bytes, bytes + uncompSize);
            mz_free(data);
            break;
        }
        if (data)
            mz_free(data);
    }

    mz_zip_reader_end(&zip);
    return result;
}

// ---- DEBUG TẠM: xem giải thích ở chỗ dùng (trong clock loop bên dưới).
// SJNES.cpp reset biến này về 0 ngay sau khi nạp ROM mới xong.
long g_debugPcLogCount = 0;
// Bộ đếm CPU cycle toàn cục cho mapper (MMC1 bỏ qua lần ghi liên tiếp).
uint64_t g_cpuCycle = 0;
// NSF: ngan sach CPU cycle toi da cho init()/play(). Ban cu cat play() o 20000 cycle,
// NSF nhieu chip (driver nang) hay vuot muc nay -> play() bi cat giua chung roi
// chay lai tu dau moi frame -> nhac lap 1 doan, khong bao gio toi doan sau.
static constexpr int NSF_INIT_MAX_CYCLES = 2000000;
static constexpr int NSF_PLAY_MAX_CYCLES = 120000;

EmuWorker::EmuWorker(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<AudioDebugChannels>("AudioDebugChannels");
    qRegisterMetaType<std::vector<AudioDebugChannels>>("std::vector<AudioDebugChannels>");
    qRegisterMetaType<NSFFile>("NSFFile");
}

EmuWorker::~EmuWorker()
{
    saveFdsDisk();
}

void EmuWorker::saveFdsDisk()
{
    if (!nes_bus.cart || fdsSavePath.isEmpty())
        return;

    auto* fds = dynamic_cast<Mapper_020*>(nes_bus.cart->pMapper.get());
    if (!fds || !fds->IsDiskDirty())
        return;

    std::vector<uint8_t> ips = fds->BuildDiskPatch();
    if (ips.empty())
    {
        // Mọi byte đã quay về như đĩa gốc -> không cần file save.
        QFile::remove(fdsSavePath);
        fds->ClearDiskDirty();
        return;
    }

    QSaveFile f(fdsSavePath); // ghi nguyên tử: crash giữa chừng không làm hỏng save cũ
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(reinterpret_cast<const char*>(ips.data()), qint64(ips.size()));
    if (f.commit())
        fds->ClearDiskDirty();
}

void EmuWorker::start()
{
    if (!timer) {
        timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        connect(timer, &QTimer::timeout, this, &EmuWorker::runFrame);
    }
    timer->start(1);
}
void EmuWorker::stop()
{
    if (timer) timer->stop();
    saveFdsDisk();
}

// Tim va doc BIOS FDS (DISKSYS.ROM). Tim khong phan biet hoa/thuong, o thu muc
// cua exe, thu muc chua file .fds, va thu muc lam viec hien tai. Chap nhan
// 8192 byte, hoac 8208 byte (co header 16 byte thi bo 16 byte dau).
// Tra ve rong neu khong tim thay / kich thuoc khong hop le (errorOut noi ly do).
static std::vector<uint8_t> LoadFdsBios_Worker(const QString& fdsPath, QString& errorOut)
{
    QStringList dirs;
    dirs << QCoreApplication::applicationDirPath()
        << QFileInfo(fdsPath).absolutePath()
        << QDir::currentPath();

    for (const QString& dirPath : dirs)
    {
        QDir dir(dirPath);
        const QFileInfoList files = dir.entryInfoList(QDir::Files);
        for (const QFileInfo& fi : files)
        {
            if (fi.fileName().compare("disksys.rom", Qt::CaseInsensitive) != 0)
                continue;

            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly))
            {
                errorOut = "Khong mo duoc " + fi.absoluteFilePath();
                return {};
            }
            QByteArray raw = f.readAll();
            if (raw.size() == 8208)
                raw.remove(0, 16);
            if (raw.size() != 8192)
            {
                errorOut = QString("DISKSYS.ROM sai kich thuoc (%1 byte, can 8192): %2")
                    .arg(raw.size()).arg(fi.absoluteFilePath());
                return {};
            }
            return std::vector<uint8_t>(raw.begin(), raw.end());
        }
    }

    errorOut = "Khong tim thay DISKSYS.ROM. Hay dat file cung thu muc voi file .exe (hoac cung thu muc voi file .fds).";
    return {};
}

void EmuWorker::loadRom(QString fileName, bool smoothSaw)
{
    // Toàn bộ hàm này giờ chạy TRÊN emuThread (được gọi qua QueuedConnection
    // từ SJNES::loadRomFile), không còn chạy trên UI thread nữa - đây là fix
    // cho hiện tượng "đơ 1 tẹo lúc chọn ROM xong": trước kia đọc file + parse
    // Cartridge + 2 lần BlockingQueuedConnection đều nằm trên UI thread nên
    // Qt event loop bị treo trong lúc đó.

    if (timer) timer->stop();

    // Tat NSF mode ngay tren emuThread, truoc khi lam gi khac - tranh race
    // voi UI thread (truoc day 2 dong nay chay tu SJNES::loadRomFile() tren
    // UI thread trong khi emuThread van co the dang clock, gay access
    // violation ngau nhien luc mo ROM).
    nes_bus.nsfMode = false;
    nes_bus.DisableNSFMode();

    system_clock_counter = 0;
    dma_dummy_counter = 0;
    dma_cycles_needed = 513;
    cpu_cycle_parity = 0;
    nmi_write_delay = 0;

    for (int i = 0; i < 2048; i++)
        nes_bus.ram[i] = 0x00;

    std::shared_ptr<Cartridge> cart = nullptr;

    // Luu save cua game dang choi truoc khi thay bang ROM/cartridge moi,
    // neu khong Kirby (hoac bat ky game co battery nao) dang choi do se mat
    // save khi ban mo mot ROM khac ma chua kip ghi ra dia.
    if (nes_bus.cart && nes_bus.cart->bBattery)
        nes_bus.cart->SavePRGRAMToFile();
    saveFdsDisk(); // đĩa FDS của game đang chơi
    fdsSavePath.clear();

    if (fileName.endsWith(".zip", Qt::CaseInsensitive))
    {
        std::vector<uint8_t> romData = LoadNesFromZip_Worker(fileName);
        if (romData.empty())
        {
            emit romLoadResult(false, fileName, "Khong tim thay file .nes trong file .zip.", 999, 0);
            if (timer) timer->start(1);
            return;
        }
        cart = std::make_shared<Cartridge>(romData);
    }
    else
    {
        cart = std::make_shared<Cartridge>(fileName.toStdString());
    }

    if (cart == nullptr || !cart->ImageValid() || cart->pMapper == nullptr)
    {
        emit romLoadResult(false, fileName, "Khong the nap ROM hoac Mapper chua duoc ho tro.", 999, 0);
        if (timer) timer->start(1);
        return;
    }

    // FDS: nap BIOS (DISKSYS.ROM) TRUOC khi reset CPU, vi CPU lay reset vector
    // ($FFFC) tu BIOS.
    if (auto fdsMapper = dynamic_cast<Mapper_020*>(cart->pMapper.get()))
    {
        if (!fdsMapper->HasBIOS())
        {
            QString biosError;
            std::vector<uint8_t> biosData = LoadFdsBios_Worker(fileName, biosError);
            if (biosData.empty())
            {
                emit romLoadResult(false, fileName, biosError, 999, 0);
                if (timer) timer->start(1);
                return;
            }
            fdsMapper->SetBIOS(biosData);
        }

        // Nạp save đĩa cũ (nếu có). Patch hỏng/không khớp thì bỏ qua, đĩa giữ nguyên bản gốc.
        fdsSavePath = FdsSavePathFor(fileName);
        QFile saveFile(fdsSavePath);
        if (saveFile.open(QIODevice::ReadOnly))
        {
            QByteArray raw = saveFile.readAll();
            std::vector<uint8_t> ips(raw.begin(), raw.end());
            if (!fdsMapper->ApplyDiskPatch(ips))
                qWarning("[FDS] File save dia khong hop le, bo qua: %s", qPrintable(fdsSavePath));
        }
    }

    // Tinh duong dan file .sav (cung ten voi fileName, doi duoi thanh .sav)
    // va tu nap save cu neu game co battery va file da ton tai.
    cart->InitBatterySave(fileName.toStdString());

    nes_bus.insertCartridge(cart);
    nes_ppu.ConnectCartridge(cart);

    nes_ppu.reset();
    nes_bus.n_apu.reset();

    if (cart->pMapper != nullptr)
        cart->pMapper->reset();

    nes_cpu.reset();
    nes_bus.n_apu.SetSmoothSaw(smoothSaw);

    if (cart->pMapper != nullptr)
    {
        auto vrc6 = dynamic_cast<Mapper_024*>(cart->pMapper.get());
        if (vrc6)
            vrc6->setSmoothSaw(smoothSaw);
    }

    uint8_t irqLo = nes_bus.cpuRead(0xFFFE);
    uint8_t irqHi = nes_bus.cpuRead(0xFFFF);
    uint16_t irqVec = (irqHi << 8) | irqLo;

    emit romLoadResult(true, fileName, QString(), cart->nMapperID, nes_cpu.pc);

    if (!timer) {
        timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        connect(timer, &QTimer::timeout, this, &EmuWorker::runFrame);
    }
    timer->start(1);
}

void EmuWorker::runFrame()
{
    if (!machinePowered) return;
    if (nes_bus.nsfMode) { runNSFFrame(); return; }

    // Autosave dinh ky PRG-RAM (~10 giay/lan o 60fps) de tranh mat save neu
    // app bi crash hoac bi dong dot ngot (Task Manager, mat dien...), khong
    // the trong cay vao destructor cua Cartridge trong nhung truong hop do.
    static int s_autosaveFrameCounter = 0;
    if (++s_autosaveFrameCounter >= 600)
    {
        s_autosaveFrameCounter = 0;
        if (nes_bus.cart && nes_bus.cart->bBattery)
            nes_bus.cart->SavePRGRAMToFile();
        saveFdsDisk();
    }

    constexpr double NES_CPU_HZ = 1789773.0;
    constexpr double NES_PPU_CYCLES_PER_FRAME = 89342.0;
    constexpr double NES_FPS = NES_CPU_HZ / (NES_PPU_CYCLES_PER_FRAME / 3.0);
    const qint64 FRAME_NS = static_cast<qint64>(1000000000.0 / NES_FPS);

    if (!framePacer.isValid()) { framePacer.start(); nextFrameNs = framePacer.nsecsElapsed(); }
    qint64 now = framePacer.nsecsElapsed();
    if (now < nextFrameNs) return;
    if (now - nextFrameNs > FRAME_NS * 5) nextFrameNs = now;
    nextFrameNs += FRAME_NS;

    int framesToRun = fastForward ? fastForwardMultiplier.load() : 1;

    for (int ff = 0; ff < framesToRun; ff++)
    {
        dbgBatch.clear();
        static std::vector<float> audio_samples;
        audio_samples.clear();
        if (audio_samples.capacity() < 8192) audio_samples.reserve(8192);

        // Nếu UI vừa đổi sample rate (44.1<->48kHz...) qua setOutputSampleRate(),
        // reset accumulator để không cộng dồn lệch nhịp từ rate cũ.
        if (sampleRateChangePending.exchange(false))
            audio_accumulator = 0.0;
        const double currentSampleRate = outputSampleRate.load();

        // APU tự tính sampleRateHz nội bộ (cho polyBLEP + bộ lọc output thật)
        // dựa vào cycles_per_sample — nhưng biến này TRƯỚC ĐÂY không hề được
        // đồng bộ ở đâu cả, mãi mãi giữ giá trị mặc định tương đương 44100Hz
        // dù outputSampleRate đã đổi. Set lại mỗi frame cho khớp (phép chia
        // đơn giản, rẻ, không đáng lo hiệu năng).
        nes_bus.n_apu.cycles_per_sample = 1789773.0 / currentSampleRate;

        const float MASTER_VOLUME = 0.95f;
        const bool outputAudio = !fastForward && wantOutputAudio;
        const bool needAudioMix = outputAudio || wantWaveDebug;
        const bool stereo = is_stereo;

        Mapper* mapper = nullptr;
        Mapper_005* mmc5 = nullptr;
        Mapper_019* n163 = nullptr;
        Mapper_085* vrc7 = nullptr;
        Mapper_069* s5b = nullptr;
        Mapper_024* vrc6 = nullptr;
        Mapper_020* fds = nullptr;

        if (nes_bus.cart && nes_bus.cart->pMapper)
        {
            mapper = nes_bus.cart->pMapper.get();
            mmc5 = dynamic_cast<Mapper_005*>(mapper);
            n163 = dynamic_cast<Mapper_019*>(mapper);
            vrc7 = dynamic_cast<Mapper_085*>(mapper);
            s5b = dynamic_cast<Mapper_069*>(mapper);
            vrc6 = dynamic_cast<Mapper_024*>(mapper);
            fds = dynamic_cast<Mapper_020*>(mapper);
            // Giống đường NSF (runNSFFrame): đồng bộ sample rate thật cho các chip
            // tự sinh mẫu. Idempotent nên gọi mỗi frame không tốn gì. Thiếu dòng này
            // thì VRC7 (emu2413) mặc định 44100Hz sẽ lệch cao độ khi xuất 48kHz.
            if (vrc7) vrc7->SetSampleRate(currentSampleRate);
            if (n163) n163->SetSampleRate(currentSampleRate);
            if (s5b)  s5b->SetSampleRate(currentSampleRate);
        }

        AudioDebugChannels dbg;
        bool dbgFilled = false;

        auto fillDebugIfNeeded = [&]() {
            if (!wantWaveDebug) return;
            dbg = nes_bus.n_apu.GetDebugChannels();
            if (n163) {
                n163->GetN163DebugChannels(dbg.n163Wave1, dbg.n163Wave2, dbg.n163Wave3, dbg.n163Wave4,
                    dbg.n163Wave5, dbg.n163Wave6, dbg.n163Wave7, dbg.n163Wave8);
                n163->GetN163DebugPeriods(dbg.n163Period1, dbg.n163Period2, dbg.n163Period3, dbg.n163Period4,
                    dbg.n163Period5, dbg.n163Period6, dbg.n163Period7, dbg.n163Period8);
                n163->GetN163DebugPhases(dbg.n163Phase1, dbg.n163Phase2, dbg.n163Phase3, dbg.n163Phase4,
                    dbg.n163Phase5, dbg.n163Phase6, dbg.n163Phase7, dbg.n163Phase8);
            }
            else if (mmc5) {
                mmc5->GetMMC5DebugChannels(dbg.mmc5Pulse1, dbg.mmc5Pulse2, dbg.mmc5PCM);
                mmc5->GetMMC5DebugPeriods(dbg.mmc5Pulse1Period, dbg.mmc5Pulse2Period);
                mmc5->GetMMC5DebugDuty(dbg.mmc5Pulse1Duty, dbg.mmc5Pulse2Duty);
            }
            else if (vrc7) {
                vrc7->GetVrc7DebugChannels(dbg.vrc7Wave1, dbg.vrc7Wave2, dbg.vrc7Wave3, dbg.vrc7Wave4, dbg.vrc7Wave5, dbg.vrc7Wave6);
                vrc7->GetVrc7DebugPeriods(dbg.vrc7Wave1Period, dbg.vrc7Wave2Period, dbg.vrc7Wave3Period, dbg.vrc7Wave4Period, dbg.vrc7Wave5Period, dbg.vrc7Wave6Period);
                vrc7->GetVrc7DebugPhases(dbg.vrc7Wave1Phase, dbg.vrc7Wave2Phase, dbg.vrc7Wave3Phase, dbg.vrc7Wave4Phase, dbg.vrc7Wave5Phase, dbg.vrc7Wave6Phase);
            }
            else if (s5b) {
                s5b->GetExpansionDebugChannels(dbg.s5bToneA, dbg.s5bToneB, dbg.s5bToneC);
                s5b->GetS5BDebugPeriods(dbg.s5bToneAPeriod, dbg.s5bToneBPeriod, dbg.s5bToneCPeriod);
                s5b->GetS5BDebugDuty(dbg.s5bToneADuty, dbg.s5bToneBDuty, dbg.s5bToneCDuty);
            }
            else if (vrc6) {
                vrc6->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
                vrc6->GetVRC6DebugPeriods(dbg.vrc6Pulse1Period, dbg.vrc6Pulse2Period, dbg.vrc6SawPeriod);
                vrc6->GetVRC6DebugDuty(dbg.vrc6Pulse1Duty, dbg.vrc6Pulse2Duty);
            }
            else if (fds) {
                fds->GetFdsDebugChannels(dbg.fdsWave);
                fds->GetFdsDebugPeriod(dbg.fdsWavePeriod, float(currentSampleRate));
            }
            else if (mapper) {
                mapper->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
            }
            dbgBatch.push_back(dbg);
            dbgFilled = true;
            };

        bool ppuFrameDone = false;
        do
        {
            nes_bus.ppu->Step();
            if (nes_bus.ppu->nmi_requested) { 
                nes_bus.ppu->nmi_requested = false; 
            nes_cpu.nmi(system_clock_counter % 3 == 1); }
            if (nes_bus.ppu->nmi_write_pending)
            {
                nes_bus.ppu->nmi_write_pending = false;
                nmi_write_delay = 2; 
            }

            if (system_clock_counter % 3 == 0)
            {
                if (nmi_write_delay > 0)
                {
                    nmi_write_delay--;
                    if (nmi_write_delay == 0) nes_cpu.nmi();
                }

                if (mapper) {
                    mapper->irqStep();
                    if (mapper->irqState()) nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                    else nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
                }

                // BUG FIX (DMA parity): ngưỡng lấy từ dma_cycles_needed (513 hoặc 514,
                // được tính ngay dưới đây lúc DMA vừa bắt đầu) thay vì cứng 512/513 như
                // trước - xem giải thích đầy đủ ở chỗ gán dma_cycles_needed bên dưới.
                if (nes_bus.dma_transfer) {
                    if (dma_dummy_counter < dma_cycles_needed - 1) dma_dummy_counter++;
                    else { nes_bus.dma_transfer = false; dma_dummy_counter = 0; }
                }
                else if (nes_bus.dmc_stall_cycles > 0) {
                    // DMC DMA: CPU dung yen, PPU/APU/mapper van chay binh thuong.
                    nes_bus.dmc_stall_cycles--;
                }
                else {
                    // ---- DEBUG TẠM: log PC mỗi khi bắt đầu lệnh mới, để so
                    // trực tiếp với Mesen trace (địa chỉ E1CE/EE19/EE20/E68B/
                    // E6A0/E7A0...). g_debugPcLogCount là biến GLOBAL (khai
                    // báo cuối file này), được SJNES.cpp reset về 0 ngay sau
                    // khi nạp ROM mới xong - nhờ vậy ngân sách log luôn bắt
                    // đầu đúng lúc mở file .fds, không bị "ăn" hết bởi các
                    // cycle chạy trước đó.
                    if (nes_cpu.complete() && g_debugPcLogCount < 300000) {
                        // TẠM TẮT log PC (gây lag nhiều) - chỉ đếm, không in.
                        // Bật lại bằng cách bỏ comment dòng qDebug bên dưới khi cần.
                        //qDebug("[PC] $%04X A:%02X X:%02X Y:%02X", nes_cpu.pc, nes_cpu.a, nes_cpu.x, nes_cpu.y);
                        g_debugPcLogCount++;
                    }
                    nes_cpu.clock();

                    // NMI suppression race (case CÙNG dot / 1 dot SAU lúc VBL
                    // set - xem PPU::cancel_pending_nmi): nếu lệnh CPU vừa
                    // chạy ở trên vừa đọc $2002 rơi đúng 1 trong 2 case đó,
                    // cpu.nmi() ĐÃ được dispatch từ trước (xem khối check
                    // nmi_requested phía trên, chạy trước cpu.clock() này) -
                    // phải huỷ ngược lại NGAY tại đây, trước khi CPU kịp tới
                    // ranh giới lệnh kế tiếp để poll nmi_pending, nếu không
                    // NMI vẫn cứ chạy dù $2002 báo đã suppress (fix test
                    // 06-suppression).
                    if (nes_bus.ppu->cancel_pending_nmi)
                    {
                        nes_bus.ppu->cancel_pending_nmi = false;
                        nes_cpu.CancelPendingNmi();
                    }

                    // BUG FIX (DMA parity): nếu lệnh CPU vừa chạy ghi $4014 (bật
                    // nes_bus.dma_transfer lên true NGAY TRONG lệnh này), phần cứng
                    // thật cần 513 cycle nếu CHÍNH cycle này (cycle mà CPU "thấy" yêu
                    // cầu DMA) là CHẴN, hoặc 514 nếu LẺ (cộng thêm 1 cycle "align").
                    // Bản cũ luôn cố định 513 (không bao giờ lên 514) -> thiếu đúng 1
                    // cycle mỗi khi $4014 rơi vào 1 cycle lẻ. Vì PPU cắt bớt 1 dot ở
                    // frame lẻ (odd-frame skip), lệch pha CPU-PPU trôi dần qua từng
                    // frame -> có frame $4014 rơi lẻ, có frame thì không -> giật CHẬP
                    // CHỜN (không phải frame nào cũng giật), đúng kiểu Recca đang gặp.
                    if (nes_bus.dma_transfer)
                    {
                        dma_cycles_needed = (cpu_cycle_parity % 2 == 0) ? 513 : 514;
                    }
                }

                // Đếm parity CPU cycle THẬT (0,1,2,3,...) - dùng riêng cho việc tính
                // dma_cycles_needed ở trên, độc lập với system_clock_counter (vốn đếm
                // theo dot PPU, chia 3 mới ra CPU cycle nhưng không phản ánh đúng "cycle
                // thứ mấy" theo nghĩa NESDev dùng cho luật 513/514).
                cpu_cycle_parity++; g_cpuCycle++;

                nes_bus.n_apu.Step();
                if (mmc5) mmc5->ClockAudio();

                audio_accumulator += currentSampleRate / 1789773.0;
                if (audio_accumulator >= 1.0)
                {
                    audio_accumulator -= 1.0;
                    if (needAudioMix)
                    {
                        if (stereo)
                        {
                            float left = 0.0f, right = 0.0f;
                            nes_bus.n_apu.GetOutputSampleStereo(left, right);
                            float expL = 0.0f, expR = 0.0f;
                            if (mapper) mapper->GetExpansionAudioStereo(expL, expR);
                            if (n163) { expL *= 2.0f; expR *= 2.0f; }
                            fillDebugIfNeeded();
                            left += expL; right += expR;
                            // Filter chain thật (2 highpass + 1 lowpass) áp SAU khi cộng
                            // expansion audio, đúng với đường tín hiệu analog phần cứng thật.
                            nes_bus.n_apu.ApplyOutputFilterStereo(left, right);
                            left = std::tanh(left * MASTER_VOLUME);
                            right = std::tanh(right * MASTER_VOLUME);

                            if (audioEQ.enabled)
                            {
                                left = audioEQ.ProcessLeft(left);
                                right = audioEQ.ProcessRight(right);
                            }

                            if (outputAudio) { audio_samples.push_back(left); audio_samples.push_back(right); }
                        }
                        else
                        {
                            float sample = nes_bus.n_apu.GetOutputSample();
                            float exp = mapper ? mapper->GetExpansionAudio() : 0.0f;
                            if (n163) exp *= 2.0f;
                            fillDebugIfNeeded();
                            sample += exp;
                            sample = nes_bus.n_apu.ApplyOutputFilterMono(sample);
                            sample = std::tanh(sample * MASTER_VOLUME);
                            if (audioEQ.enabled)
                            {
                                sample = audioEQ.ProcessLeft(sample);
                            }
                            if (outputAudio) audio_samples.push_back(sample);
                        }
                    }
                }
            }

            system_clock_counter++;
            ppuFrameDone = nes_bus.ppu->IsFrameBoundary();
        } while (!ppuFrameDone);

        int extraScanlines = nes_ppu.GetExtraScanlinesBeforeNMI();
        int extraCpuCycles = (extraScanlines * 341) / 3;
        for (int i = 0; i < extraCpuCycles; i++)
        {
            if (mapper) {
                mapper->irqStep();
                if (mapper->irqState()) nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                else nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
            }
            // BUG FIX (DMA parity) - xem giải thích đầy đủ ở vòng lặp chính phía trên.
            if (nes_bus.dma_transfer) {
                if (dma_dummy_counter < dma_cycles_needed - 1) dma_dummy_counter++;
                else { nes_bus.dma_transfer = false; dma_dummy_counter = 0; }
            }
            else {
                nes_cpu.clock();
                if (nes_bus.dma_transfer)
                    dma_cycles_needed = (cpu_cycle_parity % 2 == 0) ? 513 : 514;
            }
            cpu_cycle_parity++; g_cpuCycle++;
            if (mmc5) mmc5->ClockAudio();
        }

        if (outputAudio && !audio_samples.empty())
            emit audioReady(audio_samples, stereo);

        if (!dbgBatch.empty())
            emit debugChannelsReady(dbgBatch);

        bool shouldOutputVideoFrame = video60fps || ((videoFrameCounter & 1) == 0);

        if (shouldOutputVideoFrame)
        {
            QImage frameImage(
                reinterpret_cast<const uchar*>(nes_ppu.GetScreenBuffer()),
                256, 240, QImage::Format_RGB32
            );
            emit frameReady(frameImage.copy());
        }

        emit gameFrameTicked();
        if (nes_bus.inputPolledThisFrame)
            emit realGameFrameTicked();
        nes_bus.inputPolledThisFrame = false;

        videoFrameCounter++;

        uint8_t p1 = controllerState1;
        uint8_t p2 = controllerState2;

        if (holdturboA && (p1 & 0x80)) { if ((videoFrameCounter & 1) == 0) p1 &= ~0x80; }
        if (holdturboB && (p1 & 0x40)) { if ((videoFrameCounter & 1) == 0) p1 &= ~0x40; }

        nes_bus.controller_state = p1;
        nes_bus.controller_state2 = p2;
        LogClear();
    }
}

void EmuWorker::loadNSF(NSFFile nsf)
{
    stop();

    // Luu save cua game dang choi truoc khi huy cartridge de chuyen sang
    // che do NSF (nes_bus.cart = nullptr o duoi), neu khong se mat save.
    if (nes_bus.cart && nes_bus.cart->bBattery)
        nes_bus.cart->SavePRGRAMToFile();
    // (stop() ở trên đã lưu đĩa FDS nếu có thay đổi)
    fdsSavePath.clear();

    currentNSF = std::move(nsf);
    const NSFInfo& info = currentNSF.Info();

    currentNSFSong = info.startingSong;
    nsfAudioAccumulator = 0.0;
    system_clock_counter = 0;
    dma_dummy_counter = 0;
    dma_cycles_needed = 513;
    cpu_cycle_parity = 0;
    nmi_write_delay = 0;

    for (int i = 0; i < 2048; i++)
        nes_bus.ram[i] = 0x00;

    nes_bus.cart = nullptr;
    nes_bus.n_apu.reset();

    nes_bus.EnableNSFMode(info.loadAddress, currentNSF.ProgramData(), info.bankSwitch);

    nsfVRC6.reset(); nsfS5B.reset(); nsfVRC7.reset(); nsfMMC5.reset(); nsfN163.reset(); nsfFDS.reset();
    nes_bus.nsfExpansionWrite = nullptr;
    nes_bus.nsfExpansionRead = nullptr;

    const bool useVRC6 = currentNSF.UsesVRC6();
    const bool useS5B = currentNSF.UsesS5B();
    const bool useVRC7 = currentNSF.UsesVRC7();
    const bool useMMC5 = currentNSF.UsesMMC5();
    const bool useN163 = currentNSF.UsesN163();
    const bool useFDS = currentNSF.UsesFDS(); // NOTE: nếu NSFFile chưa có hàm này,
    // cần thêm cờ tương ứng bit FDS trong
    // header NSF (thường là bit2 của chip flags).

    if (useVRC6) { nsfVRC6 = std::make_unique<Mapper_024>(1, 1); nsfVRC6->reset(); emit nsfConsoleMessage("NSF VRC6 audio: ON"); }
    if (useS5B) { nsfS5B = std::make_unique<Mapper_069>(1, 1); nsfS5B->reset();  emit nsfConsoleMessage("NSF S5B audio: ON"); }
    if (useVRC7) { nsfVRC7 = std::make_unique<Mapper_085>(1, 1); nsfVRC7->reset(); emit nsfConsoleMessage("NSF VRC7 audio: ON"); }
    if (useN163) {
        nsfN163 = std::make_unique<Mapper_019>(1, 1);
        nsfN163->reset();
        uint32_t dummy = 0;
        nsfN163->cpuMapWrite(0xE000, dummy, 0x00);
        emit nsfConsoleMessage("NSF N163 audio: ON");
    }
    if (useMMC5) { nsfMMC5 = std::make_unique<Mapper_005>(1, 1); nsfMMC5->reset(); emit nsfConsoleMessage("NSF MMC5 audio: ON"); }
    if (useFDS) {
        nsfFDS = std::make_unique<Mapper_020>(1, 1);
        nsfFDS->reset();
        // NSF FDS tune không nhất thiết ghi $4023 (bit "sound regs enabled")
        // như game thật qua BIOS - bật thẳng để chắc chắn kênh audio hoạt động.
        nsfFDS->EnableForNSF();
        emit nsfConsoleMessage("NSF FDS audio: ON");
    }

    emit nsfExpansionDetected(useVRC6, useS5B, useVRC7, useMMC5, useN163, useFDS);

    if (nsfMMC5 || nsfN163 || nsfFDS)
    {
        nes_bus.nsfExpansionRead = [this](uint16_t addr, uint8_t& data) -> bool {
            if (nsfMMC5) {
                if (addr == 0x5015 || addr == 0x5204 || addr == 0x5205 || addr == 0x5206 ||
                    (addr >= 0x5C00 && addr <= 0x5FFF))
                    return nsfMMC5->cpuReadRegister(addr, data);
            }
            if (nsfN163) {
                if (addr >= 0x4800 && addr <= 0x4FFF)
                    return nsfN163->cpuReadRegister(addr, data);
            }
            if (nsfFDS) {
                if ((addr >= 0x4040 && addr <= 0x407F) || addr == 0x4090 || addr == 0x4092)
                    return nsfFDS->cpuReadRegister(addr, data);
            }
            return false;
            };
    }

    if (nsfVRC6 || nsfS5B || nsfVRC7 || nsfMMC5 || nsfN163 || nsfFDS)
    {
        nes_bus.nsfExpansionWrite = [this](uint16_t addr, uint8_t data) -> bool {
            uint32_t mapped_addr = 0;
            if (nsfMMC5) {
                if ((addr >= 0x5000 && addr <= 0x5015) || addr == 0x5205 || addr == 0x5206 || (addr >= 0x5C00 && addr <= 0x5FF5)) { // + ExRAM $5C00-$5FF5 (doc da di qua MMC5, ghi cung phai)
                    nsfMMC5->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC6) {
                if ((addr >= 0x9000 && addr <= 0x9003) || (addr >= 0xA000 && addr <= 0xA003) || (addr >= 0xB000 && addr <= 0xB003)) {
                    nsfVRC6->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfN163) {
                if ((addr >= 0x4800 && addr <= 0x4FFF) || (addr >= 0xF800 && addr <= 0xFFFF)) { // chỉ Data Port + Address Port (audio), bỏ CHR-bank/IRQ range không dùng trong NSF
                    nsfN163->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC7) {
                if (addr == 0x9010 || addr == 0x9030) {
                    nsfVRC7->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfS5B) {
                if (addr >= 0xC000 && addr <= 0xFFFF) {
                    nsfS5B->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfFDS) {
                // Chỉ audio regs ($4040-$408A) - KHÔNG route $4020-$4033 (disk)
                // vào đây, vì NSF FDS tune chỉ dùng phần audio, disk system
                // không liên quan (đúng như đã bàn ở đầu cuộc trò chuyện).
                if (addr >= 0x4040 && addr <= 0x408A) {
                    nsfFDS->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            return false;
            };
    }

    // KHONG de nes_cpu.reset() tu doc vector RESET ($FFFC/$FFFD) qua bus khi
    // dang o NSF bank mode: luc nay nsfBanks[7] (slot cho $F000-$FFFF) co the
    // dang chua gia tri rac/khong hop le (vd bank=15) vi chua duoc chuong
    // trinh NSF chu dong ghi lai qua $5FF8-$5FFF. Doc $FFFC/$FFFD se tinh
    // offset ra ngoai nsfBankData -> tra ve toan 0x00, khong gay crash nhung
    // co the lam sai cac buoc reset noi bo cua CPU (SP/flags) truoc khi
    // callNSFRoutine() kip gan lai PC dung. Ghi de PC ngay sau reset() de
    // tranh dung gia tri "rac" nay:
    nes_cpu.reset();
    nes_cpu.pc = info.initAddress; // ghi de ngay, khong dung PC doc tu vector $FFFC/$FFFD (co the sai o NSF bank mode)
    nes_bus.cpuWrite(0x4015, 0x1F);
    nes_bus.cpuWrite(0x4017, 0x40);

    std::vector<float> dummyAudio;
    dummyAudio.reserve(4096);
    uint8_t songIndex = static_cast<uint8_t>(currentNSFSong - 1);
    if (callNSFRoutine(info.initAddress, songIndex, 0, NSF_INIT_MAX_CYCLES, dummyAudio) >= NSF_INIT_MAX_CYCLES)
        emit nsfConsoleMessage(QString("NSF: init() khong return sau %1 cycle (PC=$%2)").arg(NSF_INIT_MAX_CYCLES).arg(QString::number(nes_cpu.pc, 16).rightJustified(4, '0').toUpper()));

    emit nsfConsoleMessage(QString("Title: %1").arg(QString::fromStdString(info.songName)));
    emit nsfConsoleMessage(QString("Artist: %1").arg(QString::fromStdString(info.artist)));
    emit nsfTrackChanged(currentNSFSong, info.totalSongs);

    start();
}

int EmuWorker::callNSFRoutine(uint16_t address, uint8_t a, uint8_t x, int maxCpuCycles, std::vector<float>& audioSamples)
{
    nes_cpu.a = a;
    nes_cpu.x = x;
    nes_cpu.y = 0;
    nes_cpu.stkp = 0xFD;
    nes_bus.cpuWrite(0x01FE, 0xFE);
    nes_bus.cpuWrite(0x01FF, 0xFF);
    nes_cpu.pc = address;

    int cycles = 0;
    while (nes_cpu.pc != 0xFFFF && cycles < maxCpuCycles)
    {
        // (da bo qDebug "PC stuck" moi 2000 cycle: spam log lam giat/underrun audio)
        nes_cpu.clock();
        nes_bus.n_apu.Step();

        if (nsfVRC6) nsfVRC6->irqStep();
        if (nsfS5B) nsfS5B->irqStep();
        if (nsfMMC5) nsfMMC5->ClockAudio();
        if (nsfN163) nsfN163->irqStep();
        if (nsfFDS) nsfFDS->irqStep(); // clock cả disk-noop (currentSide=-1) lẫn audio thật

        nsfAudioAccumulator += outputSampleRate.load() / 1789773.0;
        if (nsfAudioAccumulator >= 1.0)
        {
            nsfAudioAccumulator -= 1.0;
            pushNSFAudioSample(audioSamples);
        }
        cycles++;
    }
    return cycles;
}

void EmuWorker::pushNSFAudioSample(std::vector<float>& audioSamples)
{
    const float MASTER_VOLUME = 0.95f;
    const float VRC6_GAIN = 1.0f, S5B_GAIN = 0.75f, VRC7_GAIN = 2.0f, MMC5_GAIN = 1.0f, N163_GAIN = 2.0f, FDS_GAIN = 1.0f;
    // NaN/inf tu 1 chip se nhiem ban bo loc output (IIR) vinh vien -> tat tieng. Chan truoc khi loc.
    auto fin = [](float v) { return std::isfinite(v) ? v : 0.0f; };
    float expL = 0.0f, expR = 0.0f;

    if (nsfVRC6) { float vL = 0, vR = 0; nsfVRC6->GetExpansionAudioStereo(vL, vR); expL += vL * VRC6_GAIN; expR += vR * VRC6_GAIN; }
    if (nsfS5B) { float v = nsfS5B->GetExpansionAudio() * S5B_GAIN; expL += v; expR += v; }
    if (nsfVRC7) { float v = nsfVRC7->GetExpansionAudio() * VRC7_GAIN; expL += v; expR += v; }
    if (nsfMMC5) { float v = nsfMMC5->GetExpansionAudio() * MMC5_GAIN; expL += v; expR += v; }
    if (nsfN163) { float v = nsfN163->GetExpansionAudio() * N163_GAIN; expL += v; expR += v; }
    if (nsfFDS) { float v = nsfFDS->GetExpansionAudio() * FDS_GAIN; expL += v; expR += v; }

    float left = 0.0f, right = 0.0f, mono = 0.0f;
    const bool stereo = is_stereo;
    if (stereo) nes_bus.n_apu.GetOutputSampleStereo(left, right);
    else mono = nes_bus.n_apu.GetOutputSample();

    if (wantWaveDebug)
    {
        AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();
        if (nsfN163) {
            nsfN163->GetN163DebugChannels(dbg.n163Wave1, dbg.n163Wave2, dbg.n163Wave3, dbg.n163Wave4, dbg.n163Wave5, dbg.n163Wave6, dbg.n163Wave7, dbg.n163Wave8);
            nsfN163->GetN163DebugPeriods(dbg.n163Period1, dbg.n163Period2, dbg.n163Period3, dbg.n163Period4, dbg.n163Period5, dbg.n163Period6, dbg.n163Period7, dbg.n163Period8);
            nsfN163->GetN163DebugPhases(dbg.n163Phase1, dbg.n163Phase2, dbg.n163Phase3, dbg.n163Phase4, dbg.n163Phase5, dbg.n163Phase6, dbg.n163Phase7, dbg.n163Phase8);
        }
        if (nsfMMC5) {
            nsfMMC5->GetMMC5DebugChannels(dbg.mmc5Pulse1, dbg.mmc5Pulse2, dbg.mmc5PCM);
            nsfMMC5->GetMMC5DebugPeriods(dbg.mmc5Pulse1Period, dbg.mmc5Pulse2Period);
            nsfMMC5->GetMMC5DebugDuty(dbg.mmc5Pulse1Duty, dbg.mmc5Pulse2Duty);
        }
        if (nsfVRC7) {
            nsfVRC7->GetVrc7DebugChannels(dbg.vrc7Wave1, dbg.vrc7Wave2, dbg.vrc7Wave3, dbg.vrc7Wave4, dbg.vrc7Wave5, dbg.vrc7Wave6);
            nsfVRC7->GetVrc7DebugPeriods(dbg.vrc7Wave1Period, dbg.vrc7Wave2Period, dbg.vrc7Wave3Period, dbg.vrc7Wave4Period, dbg.vrc7Wave5Period, dbg.vrc7Wave6Period);
            nsfVRC7->GetVrc7DebugPhases(dbg.vrc7Wave1Phase, dbg.vrc7Wave2Phase, dbg.vrc7Wave3Phase, dbg.vrc7Wave4Phase, dbg.vrc7Wave5Phase, dbg.vrc7Wave6Phase);
        }
        if (nsfS5B) {
            nsfS5B->GetExpansionDebugChannels(dbg.s5bToneA, dbg.s5bToneB, dbg.s5bToneC);
            nsfS5B->GetS5BDebugPeriods(dbg.s5bToneAPeriod, dbg.s5bToneBPeriod, dbg.s5bToneCPeriod);
            nsfS5B->GetS5BDebugDuty(dbg.s5bToneADuty, dbg.s5bToneBDuty, dbg.s5bToneCDuty);
        }
        if (nsfVRC6) {
            nsfVRC6->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
            nsfVRC6->GetVRC6DebugPeriods(dbg.vrc6Pulse1Period, dbg.vrc6Pulse2Period, dbg.vrc6SawPeriod);
            nsfVRC6->GetVRC6DebugDuty(dbg.vrc6Pulse1Duty, dbg.vrc6Pulse2Duty);
        }
        if (nsfFDS) {
            nsfFDS->GetFdsDebugChannels(dbg.fdsWave);
            nsfFDS->GetFdsDebugPeriod(dbg.fdsWavePeriod, float(outputSampleRate.load()));
        }
        dbgBatch.push_back(dbg);
    }

    if (stereo)
    {
        left = fin(left + fin(expL)); right = fin(right + fin(expR));
        nes_bus.n_apu.ApplyOutputFilterStereo(left, right);
        // tanh (giong mono va duong ROM thuong) thay vi clamp cung: nhieu chip cong lai
        // de vuot 1.0 -> clamp gay mop/mean tieng.
        left = fin(std::tanh(left * MASTER_VOLUME));
        right = fin(std::tanh(right * MASTER_VOLUME));
        if (audioEQ.enabled)
        {
            left = audioEQ.ProcessLeft(left);
            right = audioEQ.ProcessRight(right);
        }
        audioSamples.push_back(left);
        audioSamples.push_back(right);
    }
    else
    {
        mono = fin(mono + (expL + expR) * 0.5f);
        mono = nes_bus.n_apu.ApplyOutputFilterMono(mono);
        mono = std::tanh(mono * MASTER_VOLUME);
        if (audioEQ.enabled)
        {
            mono = audioEQ.ProcessLeft(mono);
        }
        audioSamples.push_back(mono);
    }
}

void EmuWorker::runNSFFrame()
{
    constexpr double CPU_HZ = 1789773.0;
    constexpr double NSF_FPS = 60.0988;
    const qint64 FRAME_NS = static_cast<qint64>(1000000000.0 / NSF_FPS);

    if (!nsfFramePacer.isValid()) { nsfFramePacer.start(); nsfNextFrameNs = nsfFramePacer.nsecsElapsed(); }
    qint64 now = nsfFramePacer.nsecsElapsed();
    if (now < nsfNextFrameNs) return;
    if (now - nsfNextFrameNs > FRAME_NS * 5) nsfNextFrameNs = now;
    nsfNextFrameNs += FRAME_NS;

    dbgBatch.clear();
    std::vector<float> audioSamples;
    audioSamples.reserve(is_stereo ? 2048 : 1024);

    // Đồng bộ cycles_per_sample cho APU trước khi chạy — xem giải thích tương
    // tự ở runFrame(): nếu không set, sampleRateHz nội bộ của APU (dùng cho
    // ApplyOutputFilterMono/Stereo) sẽ mãi mãi tính theo 44100Hz mặc định dù
    // outputSampleRate đã đổi.
    nes_bus.n_apu.cycles_per_sample = 1789773.0 / outputSampleRate.load();

    // NSF không đi qua mapper của cartridge nên APU không tự đồng bộ được:
    // báo rate thật cho VRC7 (emu2413) và N163 (chu kỳ scope) ở đây.
    if (nsfVRC7) nsfVRC7->SetSampleRate(outputSampleRate.load());
    if (nsfN163) nsfN163->SetSampleRate(outputSampleRate.load());
    if (nsfS5B) nsfS5B->SetSampleRate(outputSampleRate.load());

    const NSFInfo& info = currentNSF.Info();
    constexpr int CPU_CYCLES_PER_FRAME = 29780;

    int usedCycles = callNSFRoutine(info.playAddress, 0, 0, NSF_PLAY_MAX_CYCLES, audioSamples);

    // Canh bao (toi da ~1 lan / 10 giay) neu play() khong chiu return -> nhac bi dung/lap.
    static int s_playOverrunWarnCooldown = 0;
    if (s_playOverrunWarnCooldown > 0) s_playOverrunWarnCooldown--;
    if (usedCycles >= NSF_PLAY_MAX_CYCLES && s_playOverrunWarnCooldown == 0)
    {
        emit nsfConsoleMessage(QString("NSF: play() khong return sau %1 cycle (PC=$%2)").arg(NSF_PLAY_MAX_CYCLES).arg(QString::number(nes_cpu.pc, 16).rightJustified(4, '0').toUpper()));
        s_playOverrunWarnCooldown = 600;
    }

    // play() vuot 1 frame (29780 cycle) -> tinh no vao "no" de cac frame sau bot cycle
    // roi, tranh sinh qua nhieu audio so voi thoi gian that (tran buffer).
    static int s_nsfCycleDebt = 0;
    int remainCycles = CPU_CYCLES_PER_FRAME - usedCycles - s_nsfCycleDebt;
    if (remainCycles < 0)
    {
        s_nsfCycleDebt = std::min(-remainCycles, CPU_CYCLES_PER_FRAME * 4);
        remainCycles = 0;
    }
    else
    {
        s_nsfCycleDebt = 0;
    }

    for (int i = 0; i < remainCycles; i++)
    {
        nes_bus.n_apu.Step();
        if (nsfVRC6) nsfVRC6->irqStep();
        if (nsfS5B) nsfS5B->irqStep();
        if (nsfMMC5) nsfMMC5->ClockAudio();
        if (nsfN163) nsfN163->irqStep();
        if (nsfFDS) nsfFDS->irqStep();

        nsfAudioAccumulator += outputSampleRate.load() / CPU_HZ;
        if (nsfAudioAccumulator >= 1.0)
        {
            nsfAudioAccumulator -= 1.0;
            pushNSFAudioSample(audioSamples);
        }
    }

    if (!audioSamples.empty())
        emit audioReady(audioSamples, is_stereo);

    if (!dbgBatch.empty())
        emit debugChannelsReady(dbgBatch);

    emit gameFrameTicked();
    emit realGameFrameTicked();
}

void EmuWorker::restartCurrentNSFTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();

    nsfAudioAccumulator = 0.0;
    for (int i = 0; i < 2048; i++) nes_bus.ram[i] = 0x00;
    nes_bus.EnableNSFMode(info.loadAddress, currentNSF.ProgramData(), info.bankSwitch);

    if (nsfVRC6) nsfVRC6->reset();
    if (nsfS5B) nsfS5B->reset();
    if (nsfVRC7) nsfVRC7->reset();
    if (nsfMMC5) nsfMMC5->reset();
    if (nsfN163) {
        nsfN163->reset();
        uint32_t dummy = 0;
        nsfN163->cpuMapWrite(0xE000, dummy, 0x00);
    }
    if (nsfFDS) {
        nsfFDS->reset();
        nsfFDS->EnableForNSF(); // reset() tắt soundRegsEnabled, phải bật lại
    }

    if (nsfVRC6 || nsfS5B || nsfVRC7 || nsfMMC5 || nsfN163 || nsfFDS)
    {
        nes_bus.nsfExpansionWrite = [this](uint16_t addr, uint8_t data) -> bool {
            uint32_t mapped_addr = 0;
            if (nsfMMC5) {
                if ((addr >= 0x5000 && addr <= 0x5015) || addr == 0x5205 || addr == 0x5206 || (addr >= 0x5C00 && addr <= 0x5FF5)) { // + ExRAM $5C00-$5FF5 (doc da di qua MMC5, ghi cung phai)
                    nsfMMC5->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC6) {
                if ((addr >= 0x9000 && addr <= 0x9003) || (addr >= 0xA000 && addr <= 0xA003) || (addr >= 0xB000 && addr <= 0xB003)) {
                    nsfVRC6->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfN163) {
                if ((addr >= 0x4800 && addr <= 0x4FFF) || (addr >= 0xF800 && addr <= 0xFFFF)) { // chỉ Data Port + Address Port (audio), bỏ CHR-bank/IRQ range không dùng trong NSF
                    nsfN163->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC7) {
                if (addr == 0x9010 || addr == 0x9030) {
                    nsfVRC7->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfS5B) {
                if (addr >= 0xC000 && addr <= 0xFFFF) {
                    nsfS5B->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfFDS) {
                // Chỉ audio regs ($4040-$408A) - KHÔNG route $4020-$4033 (disk).
                // BUG CŨ: nhánh này (trong restartCurrentNSFTrack) THIẾU HẲN cả
                // điều kiện nsfFDS ở guard lẫn block xử lý bên dưới - nghĩa là
                // BẤT KỲ NSF dùng FDS nào (một mình hay kết hợp chip khác) đều
                // mất tiếng FDS hoàn toàn ngay khi đổi track (next/prev), vì
                // nsfExpansionWrite bị hàm này ghi đè bằng bản không có FDS.
                if (addr >= 0x4040 && addr <= 0x408A) {
                    nsfFDS->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            return false;
            };

        nes_bus.nsfExpansionRead = nullptr;
        if (nsfMMC5 || nsfN163 || nsfFDS)
        {
            nes_bus.nsfExpansionRead = [this](uint16_t addr, uint8_t& data) -> bool {
                if (nsfMMC5) {
                    if (addr == 0x5015 || addr == 0x5204 || addr == 0x5205 || addr == 0x5206 ||
                        (addr >= 0x5C00 && addr <= 0x5FFF))
                        return nsfMMC5->cpuReadRegister(addr, data);
                }
                if (nsfN163) {
                    if (addr >= 0x4800 && addr <= 0x4FFF)
                        return nsfN163->cpuReadRegister(addr, data);
                }
                if (nsfFDS) {
                    if ((addr >= 0x4040 && addr <= 0x407F) || addr == 0x4090 || addr == 0x4092)
                        return nsfFDS->cpuReadRegister(addr, data);
                }
                return false;
                };
        }
    }

    nes_bus.cpuWrite(0x4015, 0x1F);
    nes_bus.cpuWrite(0x4017, 0x40);

    uint8_t songIndex = static_cast<uint8_t>(currentNSFSong - 1);
    std::vector<float> dummyAudio;
    dummyAudio.reserve(4096);
    if (callNSFRoutine(info.initAddress, songIndex, 0, NSF_INIT_MAX_CYCLES, dummyAudio) >= NSF_INIT_MAX_CYCLES)
        emit nsfConsoleMessage(QString("NSF: init() khong return sau %1 cycle (PC=$%2)").arg(NSF_INIT_MAX_CYCLES).arg(QString::number(nes_cpu.pc, 16).rightJustified(4, '0').toUpper()));

    emit nsfTrackChanged(currentNSFSong, info.totalSongs);
}

void EmuWorker::nsfNextTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();
    currentNSFSong++;
    if (currentNSFSong > info.totalSongs) currentNSFSong = 1;
    restartCurrentNSFTrack();
}

void EmuWorker::nsfPrevTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();
    if (currentNSFSong <= 1) currentNSFSong = info.totalSongs;
    else currentNSFSong--;
    restartCurrentNSFTrack();
}

void EmuWorker::setFdsDiskSide(int side)
{
    if (!nes_bus.cart || !nes_bus.cart->pMapper) return;
    auto* fds = dynamic_cast<Mapper_020*>(nes_bus.cart->pMapper.get());
    if (!fds) return;

    fds->InsertDisk(side);   // side < 0 hoặc ngoài khoảng -> eject
    emit fdsDiskChanged(fds->GetCurrentSide(), fds->GetSideCount());
}