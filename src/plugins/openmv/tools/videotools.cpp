#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>
#include <utils/theme/theme.h>

#include "../openmvpluginio.h"
#include "openmvtr.h"

#define VIDEO_SETTINGS_GROUP "OpenMVFFMPEG"
#define LAST_CONVERT_VIDEO_SRC_PATH "LastConvertSrcPath"
#define LAST_CONVERT_VIDEO_DST_PATH "LastConvertDstPath"
#define LAST_CONVERT_VIDEO_DST_FOLDER_PATH "LastConvertDstFolderPath"
#define LAST_CONVERT_VIDEO_DST_EXTENSION "LastConvertDstExtensionPath"
#define LAST_CONVERT_VIDEO_HRES "LastConvertVideoHRes"
#define LAST_CONVERT_VIDEO_SKIP "LastConvertVideoSkip"
#define LAST_PLAY_VIDEO_PATH "LastPlayVideoPath"
#define LAST_PLAY_RTSP_URL "LastPlayVideoUrl"
#define LAST_PLAY_RTSP_PORT "LastPlayVideoPort"
#define LAST_PLAY_RTSP_TCP "LastPlayVideoTCP"
#define LAST_SAVE_VIDEO_PATH "LastSaveVideoPath"
#define LAST_SAVE_VIDEO_HRES "LastSaveVideoHRes"
#define LAST_SAVE_VIDEO_SKIP "LastSaveVideoSkip"
#define LAST_CONVERT_TERMINAL_WINDOW_GEOMETRY "LastConvertTerminalWindowGeometry"
#define LAST_PLAY_TERMINAL_WINDOW_GEOMETRY "LastPlayTerminalWindowGeometry"

#define serializeData(fp, data, size) fp.append(data, size)

namespace OpenMV {
namespace Internal {

static QByteArray jpgToBytes(const QImage &image)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly); // always return true

    if(image.isGrayscale())
    {
        for(int y = 0; y < image.height(); y++)
        {
            for(int x = 0; x < image.width(); x++)
            {
                buf.putChar(qGray(image.pixel(x, y))); // always return true
            }
        }
    }
    else
    {
        for(int y = 0; y < image.height(); y++)
        {
            for(int x = 0; x < image.width(); x++)
            {
                QRgb pixel = image.pixel(x, y);
                int red = int(((qRed(pixel)*31)+127.5)/255)&0x1F;
                int green = int(((qGreen(pixel)*63)+127.5)/255)&0x3F;
                int blue = int(((qBlue(pixel)*31)+127.5)/255)&0x1F;
                int rgb565 = (red << 11) | (green << 5) | (blue << 0);
                buf.putChar((rgb565 >> 0) & 0xFF); // always return true
                buf.putChar((rgb565 >> 8) & 0xFF); // always return true
            }
        }
    }

    buf.close();
    return out;
}

static QByteArray getMJPEGHeader(int width, int height, uint32_t frames, uint32_t bytes, float fps)
{
    QByteArray fp;

    serializeData(fp, "RIFF", 4); // FOURCC fcc; - 0
    serializeLong(fp, 216 + (frames * 8) + bytes); // DWORD cb; size - updated on close - 1
    serializeData(fp, "AVI ", 4); // FOURCC fcc; - 2

    serializeData(fp, "LIST", 4); // FOURCC fcc; - 3
    serializeLong(fp, 192); // DWORD cb; - 4
    serializeData(fp, "hdrl", 4); // FOURCC fcc; - 5

    serializeData(fp, "avih", 4); // FOURCC fcc; - 6
    serializeLong(fp, 56); // DWORD cb; - 7
    serializeLong(fp, (!roundf(fps)) ? 0 : roundf(1000000 / fps)); // DWORD dwMicroSecPerFrame; micros - updated on close - 8
    serializeLong(fp, (!frames) ? 0 : roundf((((frames * 8) + bytes) * fps) / frames)); // DWORD dwMaxBytesPerSec; updated on close - 9
    serializeLong(fp, 4); // DWORD dwPaddingGranularity; - 10
    serializeLong(fp, 0); // DWORD dwFlags; - 11
    serializeLong(fp, frames); // DWORD dwTotalFrames; frames - updated on close - 12
    serializeLong(fp, 0); // DWORD dwInitialFrames; - 13
    serializeLong(fp, 1); // DWORD dwStreams; - 14
    serializeLong(fp, 0); // DWORD dwSuggestedBufferSize; - 15
    serializeLong(fp, width); // DWORD dwWidth; width - updated on close - 16
    serializeLong(fp, height); // DWORD dwHeight; height - updated on close - 17
    serializeLong(fp, 1000); // DWORD dwScale; - 18
    serializeLong(fp, roundf(fps * 1000)); // DWORD dwRate; rate - updated on close - 19
    serializeLong(fp, 0); // DWORD dwStart; - 20
    serializeLong(fp, (!roundf(fps)) ? 0 : roundf((frames * 1000) / fps)); // DWORD dwLength; length - updated on close - 21

    serializeData(fp, "LIST", 4); // FOURCC fcc; - 22
    serializeLong(fp, 116); // DWORD cb; - 23
    serializeData(fp, "strl", 4); // FOURCC fcc; - 24

    serializeData(fp, "strh", 4); // FOURCC fcc; - 25
    serializeLong(fp, 56); // DWORD cb; - 26
    serializeData(fp, "vids", 4); // FOURCC fccType; - 27
    serializeData(fp, "MJPG", 4); // FOURCC fccHandler; - 28
    serializeLong(fp, 0); // DWORD dwFlags; - 29
    serializeWord(fp, 0); // WORD wPriority; - 30
    serializeWord(fp, 0); // WORD wLanguage; - 30.5
    serializeLong(fp, 0); // DWORD dwInitialFrames; - 31
    serializeLong(fp, 1000); // DWORD dwScale; - 32
    serializeLong(fp, roundf(fps * 1000)); // DWORD dwRate; rate - updated on close - 33
    serializeLong(fp, 0); // DWORD dwStart; - 34
    serializeLong(fp, (!roundf(fps)) ? 0 : roundf((frames * 1000) / fps)); // DWORD dwLength; length - updated on close - 35
    serializeLong(fp, 0); // DWORD dwSuggestedBufferSize; - 36
    serializeLong(fp, 10000); // DWORD dwQuality; - 37
    serializeLong(fp, 0); // DWORD dwSampleSize; - 38
    serializeWord(fp, 0); // short int left; - 39
    serializeWord(fp, 0); // short int top; - 39.5
    serializeWord(fp, 0); // short int right; - 40
    serializeWord(fp, 0); // short int bottom; - 40.5

    serializeData(fp, "strf", 4); // FOURCC fcc; - 41
    serializeLong(fp, 40); // DWORD cb; - 42
    serializeLong(fp, 40); // DWORD biSize; - 43
    serializeLong(fp, width); // LONG biWidth; width - updated on close - 44
    serializeLong(fp, height); // LONG biHeight; height - updated on close - 45
    serializeWord(fp, 1); // WORD biPlanes; - 46
    serializeWord(fp, 24); // WORD biBitCount; - 46.5
    serializeData(fp, "MJPG", 4); // DWORD biCompression; - 47
    serializeLong(fp, 0); // DWORD biSizeImage; - 48
    serializeLong(fp, 0); // LONG biXPelsPerMeter; - 49
    serializeLong(fp, 0); // LONG biYPelsPerMeter; - 50
    serializeLong(fp, 0); // DWORD biClrUsed; - 51
    serializeLong(fp, 0); // DWORD biClrImportant; - 52

    serializeData(fp, "LIST", 4); // FOURCC fcc; - 53
    serializeLong(fp, 4 + (frames * 8) + bytes); // DWORD cb; movi - updated on close - 54
    serializeData(fp, "movi", 4); // FOURCC fcc; - 55

    return fp;
}

static QByteArray addMJPEG(uint32_t *frames, uint32_t *bytes, const QPixmap &pixmap)
{
    QByteArray fp;

    serializeData(fp, "00dc", 4); // FOURCC fcc;
    *frames += 1;

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly); // always return true
    pixmap.save(&buffer, "JPG"); // always return true
    buffer.close();

    int pad = (((data.size() + 3) / 4) * 4) - data.size();
    serializeLong(fp, data.size() + pad); // DWORD cb;
    serializeData(fp, data.data(), data.size());
    serializeData(fp, "\0\0", pad);
    *bytes += data.size() + pad;

    return fp;
}

static bool getMaxSizeAndAvgMsDelta(QFile *imageWriterFile, int *avgM, int *maxW, int *maxH, bool newPixformat)
{
    QProgressDialog progress(Tr::tr("Reading File..."), Tr::tr("Cancel"), imageWriterFile->pos() / 1024, imageWriterFile->size() / 1024, Core::ICore::dialogParent(),
        Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::CustomizeWindowHint | // Dividing by 1024 above makes sure that a 4GB max file size fits in an int.
        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowType(0)));
    progress.setWindowModality(Qt::ApplicationModal);

    QDataStream stream(imageWriterFile);
    stream.setByteOrder(QDataStream::LittleEndian);

    if(stream.atEnd())
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Reading File"),
            Tr::tr("No frames found!"));

        return false;
    }

    while(!stream.atEnd())
    {
        progress.setValue(imageWriterFile->pos() / 1024); // Dividing by 1024 makes sure that a 4GB max file size fits in an int.

        int M, W, H, BPP, S = 0;

        stream >> M;
        stream >> W;
        stream >> H;
        stream >> BPP;

        if(newPixformat)
        {
            stream >> S;

            if(stream.skipRawData(12) != 12)
            {
                qDebug() << "here1";
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Reading File"),
                    Tr::tr("File is corrupt!"));

                return false;
            }
        }

        if((M < 0) || (M > (1000 * 60 * 60 * 24)) || (W <= 0) || (W > 32767) || (H <= 0) || (H > 32767) || (BPP < 0) || (BPP > (1024 * 1204 * 1024))) // Sane limits.
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Reading File"),
                Tr::tr("File is corrupt!"));

            return false;
        }

        int size = ((getImageSize(W, H, newPixformat ? S : BPP, newPixformat, BPP) + 15) / 16) * 16;

        if(stream.skipRawData(size) != size)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Reading File"),
                Tr::tr("File is corrupt!"));

            return false;
        }

        if(M > 0) *avgM = (*avgM != -1) ? ((*avgM + M) / 2) : M;
        *maxW = qMax(*maxW, W);
        *maxH = qMax(*maxH, H);

        if(progress.wasCanceled())
        {
            return false;
        }
    }

    return true;
}

static bool convertImageWriterFileToMjpegVideoFile(QFile *mjpegVideoFile, uint32_t *frames, uint32_t *bytes, QFile *imageWriterFile, int maxW, int maxH, bool rgb565ByteReversed, bool newPixformat)
{
    QProgressDialog progress(Tr::tr("Transcoding File..."), Tr::tr("Cancel"), imageWriterFile->pos() / 1024, imageWriterFile->size() / 1024, Core::ICore::dialogParent(),
        Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::CustomizeWindowHint | // Dividing by 1024 above makes sure that a 4GB max file size fits in an int.
        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowType(0)));
    progress.setWindowModality(Qt::ApplicationModal);

    QDataStream stream(imageWriterFile);
    stream.setByteOrder(QDataStream::LittleEndian);

    if(stream.atEnd())
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Transcoding File"),
            Tr::tr("No frames found!"));

        return false;
    }

    while(!stream.atEnd())
    {
        progress.setValue(imageWriterFile->pos() / 1024); // Dividing by 1024 makes sure that a 4GB max file size fits in an int.

        int M, W, H, BPP, S = 0;

        stream >> M;
        stream >> W;
        stream >> H;
        stream >> BPP;

        if(newPixformat)
        {
            stream >> S;

            if(stream.skipRawData(12) != 12)
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    Tr::tr("Reading File"),
                    Tr::tr("File is corrupt!"));

                return false;
            }
        }

        if((M < 0) || (M > (1000 * 60 * 60 * 24)) || (W <= 0) || (W > 32767) || (H <= 0) || (H > 32767) || (BPP < 0) || (BPP > (1024 * 1204 * 1024))) // Sane limits.
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("File is corrupt!"));

            return false;
        }

        QByteArray data(getImageSize(W, H, newPixformat ? S : BPP, newPixformat, BPP), 0);

        if(stream.readRawData(data.data(), data.size()) != data.size())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("File is corrupt!"));

            return false;
        }

        QPixmap pixmap = getImageFromData(data, W, H, newPixformat ? S : BPP, rgb565ByteReversed, newPixformat, BPP).scaled(maxW, maxH, Qt::KeepAspectRatio);

        int size = 16 - (data.size() % 16);

        if((size != 16) && (stream.skipRawData(size) != size))
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("File is corrupt!"));

            return false;
        }

        QPixmap image(maxW, maxH);
        image.fill(Qt::black);

        QPainter painter;

        if(!painter.begin(&image))
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("Painter Failed!"));

            return false;
        }

        painter.drawPixmap((maxW - pixmap.width()) / 2, (maxH - pixmap.height()) / 2, pixmap);

        if(!painter.end())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("Painter Failed!"));

            return false;
        }

        QByteArray jpeg = addMJPEG(frames, bytes, image);

        if(mjpegVideoFile->write(jpeg) != jpeg.size())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoding File"),
                Tr::tr("Failed to write!"));

            return false;
        }

        if(progress.wasCanceled())
        {
            return false;
        }
    }

    return true;
}

static QString handleImageWriterFiles(const QString &path)
{
    QFile file(path);

    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray data = file.read(16);

        if((file.error() == QFile::NoError) && (data.size() == 16) && (!memcmp(data.data(), "OMV IMG STR V", 13)) && (data.at(14) == '.'))
        {
            char major = *(data.data() + 13);
            char minor = *(data.data() + 15);

            if(isdigit(major) && isdigit(minor))
            {
                int version = ((major - '0') * 10) + (minor - '0');

                if((version == 10) || (version == 11) || (version == 20))
                {
                    QFile tempFile(QDir::tempPath() + QDir::separator() + QFileInfo(file).completeBaseName() + QStringLiteral(".mjpeg"));

                    if(tempFile.open(QIODevice::WriteOnly))
                    {
                        int avgM = -1, maxW = 0, maxH = 0;

                        if(getMaxSizeAndAvgMsDelta(&file, &avgM, &maxW, &maxH, version == 20))
                        {
                            if(avgM == -1) avgM = 0;

                            if(file.seek(16))
                            {
                                QByteArray header = getMJPEGHeader(maxW, maxH, 0, 0, 0);

                                if(tempFile.write(header) == header.size())
                                {
                                    uint32_t frames = 0, bytes = 0;

                                    if(convertImageWriterFileToMjpegVideoFile(&tempFile, &frames, &bytes, &file, maxW, maxH, version == 10, version == 20))
                                    {
                                        if(tempFile.seek(0))
                                        {
                                            header = getMJPEGHeader(maxW, maxH, frames, bytes, avgM ? (1000.0 / avgM) : 0.0);

                                            if(tempFile.write(header) == header.size())
                                            {
                                                return QFileInfo(tempFile).canonicalFilePath();
                                            }
                                            else
                                            {
                                                QMessageBox::critical(Core::ICore::dialogParent(),
                                                    Tr::tr("Transcoder"),
                                                    Tr::tr("Failed to write header again!"));
                                            }
                                        }
                                        else
                                        {
                                            QMessageBox::critical(Core::ICore::dialogParent(),
                                                Tr::tr("Transcoder"),
                                                Tr::tr("Seek failed!"));
                                        }
                                    }
                                }
                                else
                                {
                                    QMessageBox::critical(Core::ICore::dialogParent(),
                                        Tr::tr("Transcoder"),
                                        Tr::tr("Failed to write header!"));
                                }
                            }
                            else
                            {
                                QMessageBox::critical(Core::ICore::dialogParent(),
                                    Tr::tr("Transcoder"),
                                    Tr::tr("Seek failed!"));
                            }
                        }
                    }
                    else
                    {
                        QMessageBox::critical(Core::ICore::dialogParent(),
                            Tr::tr("Transcoder"),
                            Tr::tr("Error: %L1!").arg(tempFile.errorString()));
                    }
                }
                else
                {
                    QMessageBox::critical(Core::ICore::dialogParent(),
                        Tr::tr("Transcoder"),
                        Tr::tr("Unsupported OpenMV ImageWriter File version!"));
                }
            }
            else
            {
                return path; // Not an ImageWriter file.
            }
        }
        else if(file.error() != QFile::NoError)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Transcoder"),
                Tr::tr("Error: %L1!").arg(file.errorString()));
        }
        else
        {
            return path; // Not an ImageWriter file.
        }
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Transcoder"),
            Tr::tr("Error: %L1!").arg(file.errorString()));
    }

    return QString();
}

static QString getInputFormats()
{
    Utils::FilePath command;
    Utils::QtcProcess process;
    process.setTimeoutS(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setProcessChannelMode(QProcess::MergedChannels);

    if(Utils::HostOsInfo::isWindowsHost())
    {
        command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/windows/bin/ffmpeg.exe"));
        process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
        process.runBlocking(Utils::EventLoopMode::On);
    }
    else if(Utils::HostOsInfo::isMacHost())
    {
        command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/mac/ffmpeg"));
        process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
        process.runBlocking(Utils::EventLoopMode::On);
    }
    else if(Utils::HostOsInfo::isLinuxHost())
    {
        if(QSysInfo::buildCpuArchitecture() == QStringLiteral("i386"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86_64/bin/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-armhf/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-arm64/bin/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
    }

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QStringList list, in = process.stdOut().split(QRegularExpression(QStringLiteral("\n|\r\n|\r")), Qt::SkipEmptyParts);

        foreach(const QString &string, in)
        {
            QRegularExpressionMatch match = QRegularExpression(QStringLiteral("\\s+E\\s+(\\w+)\\s+(.+)")).match(string);

            if(match.hasMatch())
            {
                list.append(QString(QStringLiteral("%1 (*.%2)")).arg(match.captured(2).replace(QLatin1Char('('), QLatin1Char('[')).replace(QLatin1Char(')'), QLatin1Char(']'))).arg(match.captured(1)));
            }
        }

        return list.join(QStringLiteral(";;"));
    }
    else
    {
        QMessageBox box(QMessageBox::Warning, Tr::tr("Get Input Formats"), Tr::tr("Query failed!"), QMessageBox::Ok, Core::ICore::dialogParent(),
            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
        box.setDetailedText(command.toString() + QStringLiteral("\n\n") + process.stdOut());
        box.setDefaultButton(QMessageBox::Ok);
        box.setEscapeButton(QMessageBox::Cancel);
        box.exec();

        return QString();
    }
}

static QString getOutputFormats()
{
    Utils::FilePath command;
    Utils::QtcProcess process;
    process.setTimeoutS(10);
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setProcessChannelMode(QProcess::MergedChannels);

    if(Utils::HostOsInfo::isWindowsHost())
    {
        command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/windows/bin/ffmpeg.exe"));
        process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
        process.runBlocking(Utils::EventLoopMode::On);
    }
    else if(Utils::HostOsInfo::isMacHost())
    {
        command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/mac/ffmpeg"));
        process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
        process.runBlocking(Utils::EventLoopMode::On);
    }
    else if(Utils::HostOsInfo::isLinuxHost())
    {
        if(QSysInfo::buildCpuArchitecture() == QStringLiteral("i386"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86_64/bin/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-armhf/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64"))
        {
            command = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-arm64/bin/ffmpeg"));
            process.setCommand(Utils::CommandLine(command, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-muxers")));
            process.runBlocking(Utils::EventLoopMode::On);
        }
    }

    if(process.result() == Utils::ProcessResult::FinishedWithSuccess)
    {
        QStringList list, in = process.stdOut().split(QRegularExpression(QStringLiteral("\n|\r\n|\r")), Qt::SkipEmptyParts);

        foreach(const QString &string, in)
        {
            QRegularExpressionMatch match = QRegularExpression(QStringLiteral("\\s+D\\s+(\\w+)\\s+(.+)")).match(string);

            if(match.hasMatch())
            {
                list.append(QString(QStringLiteral("%1 (*.%2)")).arg(match.captured(2).replace(QLatin1Char('('), QLatin1Char('[')).replace(QLatin1Char(')'), QLatin1Char(']'))).arg(match.captured(1)));
            }
        }

        return list.join(QStringLiteral(";;"));
    }
    else
    {
        QMessageBox box(QMessageBox::Warning, Tr::tr("Get Input Formats"), Tr::tr("Query failed!"), QMessageBox::Ok, Core::ICore::dialogParent(),
            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
        box.setDetailedText(command.toString() + QStringLiteral("\n\n") + process.stdOut());
        box.setDefaultButton(QMessageBox::Ok);
        box.setEscapeButton(QMessageBox::Cancel);
        box.exec();

        return QString();
    }
}

static bool convertVideoFile(const QString &dst, const QString &src, int scale, int skip)
{
    QString newSrc = src;
    QString newDst = dst;
    bool reformat = false;

    if(dst.toLower().endsWith(QStringLiteral(".bin")))
    {
        newDst = QDir::tempPath() + QDir::separator() + QFileInfo(dst).completeBaseName() + QStringLiteral("-%07d.jpg");
        reformat = true;
    }

    float fps, *fpsPtr = &fps;
    QRegularExpression fpsRegex(QStringLiteral("\\b(\\d+(?:\\.\\d+)?)\\s?fps\\b"));

    Utils::QtcProcess process;

    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(VIDEO_SETTINGS_GROUP));

    QDialog *dialog = new QDialog(Core::ICore::dialogParent(),
        Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
        (Utils::HostOsInfo::isLinuxHost() ? Qt::WindowDoesNotAcceptFocus : Qt::WindowType(0)) |
        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
    dialog->setAttribute(Qt::WA_ShowWithoutActivating);
    dialog->setWindowTitle(Tr::tr("Convert Video"));
    dialog->setSizeGripEnabled(true);

    if(settings->contains(QStringLiteral(LAST_CONVERT_TERMINAL_WINDOW_GEOMETRY)))
    {
        dialog->restoreGeometry(settings->value(QStringLiteral(LAST_CONVERT_TERMINAL_WINDOW_GEOMETRY)).toByteArray());
    }
    else
    {
        dialog->resize(640, 480);
    }

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    QPlainTextEdit *plainTextEdit = new QPlainTextEdit();
    plainTextEdit->setReadOnly(true);
    QFont font = TextEditor::TextEditorSettings::fontSettings().defaultFixedFontFamily();
    plainTextEdit->setFont(font);

    layout->addWidget(plainTextEdit);

    QObject::connect(&process, &Utils::QtcProcess::textOnStandardError, plainTextEdit, [plainTextEdit, fpsPtr, fpsRegex] (const QString &text) { // stdErr correct
        QRegularExpressionMatch match = fpsRegex.match(text);
        if (match.hasMatch()) *fpsPtr = match.captured(1).toFloat();
        plainTextEdit->appendPlainText(text.trimmed());
    });

    QObject::connect(&process, &Utils::QtcProcess::textOnStandardOutput, plainTextEdit, [plainTextEdit, fpsPtr, fpsRegex] (const QString &text) { // stdOut correct
        QRegularExpressionMatch match = fpsRegex.match(text);
        if (match.hasMatch()) *fpsPtr = match.captured(1).toFloat();
        plainTextEdit->appendHtml(QStringLiteral("<p style=\"color:%1\">%2</p>").
                                  arg(Utils::creatorTheme()->flag(Utils::Theme::DarkUserInterface) ? QStringLiteral("lightcoral") : QStringLiteral("coral")).
                                  arg(text.trimmed()));
    });

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(box);

    QObject::connect(dialog, &QDialog::rejected, [&process] { process.terminate(); });

    Utils::FilePath binary;
    QStringList args = QStringList() <<
                       QStringLiteral("-hide_banner") <<
                       QStringLiteral("-y") <<
                       QStringLiteral("-i") <<
                       QDir::toNativeSeparators(QDir::cleanPath(newSrc)) <<
                       QStringLiteral("-q:v") <<
                       QStringLiteral("1");
    if(scale != -1) args = args << QStringLiteral("-vf") << QString(QStringLiteral("scale=%1:-1")).arg(scale);
    args = args << QDir::toNativeSeparators(QDir::cleanPath(newDst));

    if(Utils::HostOsInfo::isWindowsHost())
    {
        binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/windows/bin/ffmpeg.exe"));
    }
    else if(Utils::HostOsInfo::isMacHost())
    {
        binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/mac/ffmpeg"));
    }
    else if(Utils::HostOsInfo::isLinuxHost())
    {
        if(QSysInfo::buildCpuArchitecture() == QStringLiteral("i386"))
        {
            binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86/ffmpeg"));
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64"))
        {
            binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86_64/bin/ffmpeg"));
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm"))
        {
            binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-armhf/ffmpeg"));
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64"))
        {
            binary = Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-arm64/bin/ffmpeg"));
        }
    }

    QString command = QString(QStringLiteral("%1 %2")).arg(binary.toString()).arg(args.join(QLatin1Char(' ')));
    plainTextEdit->appendHtml(QString(QStringLiteral("<p style=\"color:%1\">%2</p>")).
                              arg(Utils::creatorTheme()->flag(Utils::Theme::DarkUserInterface) ? QStringLiteral("lightblue") : QStringLiteral("blue")).
                              arg(command));

    dialog->show();

    process.setTimeoutS(3600); // 60 minutes...
    process.setTextChannelMode(Utils::Channel::Output, Utils::TextChannelMode::MultiLine);
    process.setTextChannelMode(Utils::Channel::Error, Utils::TextChannelMode::MultiLine);
    process.setCommand(Utils::CommandLine(binary, args));
    process.runBlocking(Utils::EventLoopMode::On, QEventLoop::AllEvents);

    settings->setValue(QStringLiteral(LAST_CONVERT_TERMINAL_WINDOW_GEOMETRY), dialog->saveGeometry());
    settings->endGroup();
    delete dialog;

    bool result = process.result() == Utils::ProcessResult::FinishedWithSuccess;

    if((process.result() != Utils::ProcessResult::FinishedWithSuccess) && (process.result() != Utils::ProcessResult::TerminatedAbnormally))
    {
        QMessageBox box(QMessageBox::Critical, Tr::tr("Convert Video"), Tr::tr("Failed to launch ffmpeg!"), QMessageBox::Ok, Core::ICore::dialogParent(),
            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
        box.setDetailedText(command + QStringLiteral("\n\n") + process.stdOut() + QStringLiteral("\n") + process.stdErr());
        box.setDefaultButton(QMessageBox::Ok);
        box.setEscapeButton(QMessageBox::Cancel);
        box.exec();
    }

    {
        QRegularExpressionMatch match = fpsRegex.match(process.readAllStandardOutput());
        if (match.hasMatch()) fps = match.captured(1).toFloat();
    }

    {
        QRegularExpressionMatch match = fpsRegex.match(process.readAllStandardError());
        if (match.hasMatch()) fps = match.captured(1).toFloat();
    }

    if(reformat && result)
    {
        QFile file(dst);

        if(file.open(QIODevice::WriteOnly))
        {
            QStringList list = QDir(QFileInfo(newDst).path()).entryList(QStringList() << (QFileInfo(dst).completeBaseName() + QStringLiteral("-*.jpg")));

            QProgressDialog progress(Tr::tr("Transcoding File..."), Tr::tr("Cancel"), 0, list.size(), Core::ICore::dialogParent(),
                Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::CustomizeWindowHint |
                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowType(0)));
            progress.setWindowModality(Qt::ApplicationModal);

            QByteArray data;
            serializeData(data, "OMV ", 4);
            serializeData(data, "IMG ", 4);
            serializeData(data, "STR ", 4);
            serializeData(data, "V1.1", 4);

            for(int i = 0, j = list.size(); i < j; i++)
            {
                progress.setValue(i);

                QFile in(QFileInfo(newDst).path() + QDir::separator() + list.at(i));

                if((!(i % (skip + 1))) && in.open(QIODevice::ReadOnly))
                {
                    QImage image = QImage::fromData(in.readAll());
                    QByteArray out = jpgToBytes(image);

                    serializeLong(data, qCeil(1000 / fps));
                    serializeLong(data, image.width());
                    serializeLong(data, image.height());
                    serializeLong(data, image.isGrayscale() ? 1 : 2);
                    data.append(out + ((out.size() % 16) ? QByteArray("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16 - (out.size() % 16)) : QByteArray()));

                    if(file.write(data) != data.size())
                    {
                        result = false;
                        QMessageBox::critical(Core::ICore::dialogParent(),
                            Tr::tr("Convert Video"),
                            Tr::tr("Unable to write to output video file!"));
                        break;
                    }

                    data.clear();
                }

                in.remove();
            }
        }
        else
        {
            result = false;
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Convert Video"),
                Tr::tr("Unable to open output video file!"));
        }
    }

    return result;
}

static bool playVideoFile(const QString &path)
{
    bool result = false;

    if(Utils::HostOsInfo::isWindowsHost())
    {
        QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.cmd"));

        if(file.open(QIODevice::WriteOnly))
        {
            QByteArray command = QString(QStringLiteral("start /wait \"ffplay.exe\" \"") +
                QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/windows/bin/ffplay.exe")).toString())) + QStringLiteral("\" -hide_banner \"") +
                QDir::toNativeSeparators(QDir::cleanPath(path)) + QStringLiteral("\"\n")).toUtf8();

            if(file.write(command) == command.size())
            {
                file.close();
                file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                result = QProcess::startDetached(QStringLiteral("cmd.exe"), QStringList()
                    << QStringLiteral("/c")
                    << QFileInfo(file).filePath());
            }
        }
    }
    else if(Utils::HostOsInfo::isMacHost())
    {
        QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

        if(file.open(QIODevice::WriteOnly))
        {
            QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/mac/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                QDir::toNativeSeparators(QDir::cleanPath(path)) + QStringLiteral("\"")).toUtf8(); // no extra new line

            if(file.write(command) == command.size())
            {
                file.close();
                file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                result = QProcess::startDetached(QStringLiteral("open"), QStringList()
                    << QStringLiteral("-a")
                    << QStringLiteral("Terminal")
                    << QFileInfo(file).filePath());
            }
        }
    }
    else if(Utils::HostOsInfo::isLinuxHost())
    {
        if(QSysInfo::buildCpuArchitecture() == QStringLiteral("i386"))
        {
            result = false;
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64"))
        {
            QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

            if(file.open(QIODevice::WriteOnly))
            {
                QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                    QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86_64/bin/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                    QDir::toNativeSeparators(QDir::cleanPath(path)) + QStringLiteral("\"\n")).toUtf8();

                if(file.write(command) == command.size())
                {
                    file.close();
                    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                    result = QProcess::startDetached(QStringLiteral("xterm"), QStringList()
                        << QStringLiteral("-e")
                        << QFileInfo(file).filePath());

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("lxterminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList()
                            << QStringLiteral("--")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("konsole"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("xfce4-terminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }
                }
            }
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm"))
        {
            result = false;
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64"))
        {
            QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

            if(file.open(QIODevice::WriteOnly))
            {
                QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                    QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-arm64/bin/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                    QDir::toNativeSeparators(QDir::cleanPath(path)) + QStringLiteral("\"\n")).toUtf8();

                if(file.write(command) == command.size())
                {
                    file.close();
                    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                    result = QProcess::startDetached(QStringLiteral("xterm"), QStringList()
                        << QStringLiteral("-e")
                        << QFileInfo(file).filePath());

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("lxterminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList()
                            << QStringLiteral("--")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("konsole"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("xfce4-terminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }
                }
            }
        }
    }

    if(!result)
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Play Video"),
            Tr::tr("Failed to launch ffplay!"));
    }

    return result;
}

static bool playRTSPStream(const QUrl &url, bool tcp)
{
    bool result = false;

    if(Utils::HostOsInfo::isWindowsHost())
    {
        QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.cmd"));

        if(file.open(QIODevice::WriteOnly))
        {
            QByteArray command = QString(QStringLiteral("start /wait \"ffplay.exe\" \"") +
                QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/windows/bin/ffplay.exe")).toString())) + QStringLiteral("\" -hide_banner \"") +
                url.toString() + (tcp ? QStringLiteral("\" -rtsp_transport tcp -fflags nobuffer\n") : QStringLiteral("\" -fflags nobuffer\n"))).toUtf8();

            if(file.write(command) == command.size())
            {
                file.close();
                file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                result = QProcess::startDetached(QStringLiteral("cmd.exe"), QStringList()
                    << QStringLiteral("/c")
                    << QFileInfo(file).filePath());
            }
        }
    }
    else if(Utils::HostOsInfo::isMacHost())
    {
        QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

        if(file.open(QIODevice::WriteOnly))
        {
            QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/mac/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                url.toString() + (tcp ? QStringLiteral("\" -rtsp_transport tcp -fflags nobuffer") : QStringLiteral("\" -fflags nobuffer"))).toUtf8(); // no extra new line

            if(file.write(command) == command.size())
            {
                file.close();
                file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                result = QProcess::startDetached(QStringLiteral("open"), QStringList()
                    << QStringLiteral("-a")
                    << QStringLiteral("Terminal")
                    << QFileInfo(file).filePath());
            }
        }
    }
    else if(Utils::HostOsInfo::isLinuxHost())
    {
        if(QSysInfo::buildCpuArchitecture() == QStringLiteral("i386"))
        {
            result = false;
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("x86_64"))
        {
            QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

            if(file.open(QIODevice::WriteOnly))
            {
                QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                    QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-x86_64/bin/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                    url.toString() + (tcp ? QStringLiteral("\" -rtsp_transport tcp -fflags nobuffer\n") : QStringLiteral("\" -fflags nobuffer\n"))).toUtf8();

                if(file.write(command) == command.size())
                {
                    file.close();
                    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                    result = QProcess::startDetached(QStringLiteral("xterm"), QStringList()
                        << QStringLiteral("-e")
                        << QFileInfo(file).filePath());

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("lxterminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList()
                            << QStringLiteral("--")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("konsole"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("xfce4-terminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }
                }
            }
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm"))
        {
            result = false;
        }
        else if(QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64"))
        {
            QFile file(QDir::tempPath() + QDir::separator() + QStringLiteral("openmvide-ffplay.sh"));

            if(file.open(QIODevice::WriteOnly))
            {
                QByteArray command = QString(QStringLiteral("#!/bin/sh\n\n\"") +
                    QDir::toNativeSeparators(QDir::cleanPath(Core::ICore::resourcePath(QStringLiteral("ffmpeg/linux-arm64/bin/ffplay")).toString())) + QStringLiteral("\" -hide_banner \"") +
                    url.toString() + (tcp ? QStringLiteral("\" -rtsp_transport tcp -fflags nobuffer\n") : QStringLiteral("\" -fflags nobuffer\n"))).toUtf8();

                if(file.write(command) == command.size())
                {
                    file.close();
                    file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
                    result = QProcess::startDetached(QStringLiteral("xterm"), QStringList()
                        << QStringLiteral("-e")
                        << QFileInfo(file).filePath());

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("lxterminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("gnome-terminal"), QStringList()
                            << QStringLiteral("--")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("konsole"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }

                    if(!result)
                    {
                        result = QProcess::startDetached(QStringLiteral("xfce4-terminal"), QStringList()
                            << QStringLiteral("-e")
                            << QFileInfo(file).filePath());
                    }
                }
            }
        }
    }

    if(!result)
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Play RTSP Stream"),
            Tr::tr("Failed to launch ffplay!"));
    }

    return result;
}

void convertVideoFileAction(const QString &drivePath)
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(VIDEO_SETTINGS_GROUP));

    QStringList srcList =
        QFileDialog::getOpenFileNames(Core::ICore::dialogParent(), Tr::tr("Convert Video Source"),
            settings->value(QStringLiteral(LAST_CONVERT_VIDEO_SRC_PATH), drivePath.isEmpty() ? QDir::homePath() : drivePath).toString(),
            Tr::tr("Video Files (*.mp4 *.*);;OpenMV ImageWriter Files (*.bin);;") + getInputFormats());

    if(srcList.size() > 1)
    {
        QString dstFolder =
        QFileDialog::getExistingDirectory(Core::ICore::dialogParent(), Tr::tr("Convert Video Output"),
            settings->value(QStringLiteral(LAST_CONVERT_VIDEO_DST_FOLDER_PATH), QDir::homePath()).toString());

        if(!dstFolder.isEmpty())
        {
            QString extensions = Tr::tr("Video Files (*.mp4 *.*);;OpenMV ImageReader Files (*.bin);;") + getOutputFormats();
            QStringList extensionsList = extensions.split(QStringLiteral(";;"));
            int index = extensionsList.indexOf(settings->value(QStringLiteral(LAST_CONVERT_VIDEO_DST_EXTENSION)).toString());

            bool ok;
            QString extension = QInputDialog::getItem(Core::ICore::dialogParent(),
                Tr::tr("Convert Video Output"), Tr::tr("Please select output format"),
                extensionsList, (index != -1) ? index : 0, true, &ok,
                Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType() : Qt::WindowCloseButtonHint));

            if(ok)
            {
                QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(\\*.*?\\.[\\w]+)")).match(extension);

                if(match.hasMatch())
                {
                    QString ext = match.captured(1).mid(1);

                    int rescale = QMessageBox::information(Core::ICore::dialogParent(),
                        Tr::tr("Convert Video"),
                        Tr::tr("Rescale the video?"),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

                    if(rescale != QMessageBox::Cancel)
                    {
                        bool ok = true;
                        int scale = -1;

                        if(rescale == QMessageBox::Yes)
                        {
                            scale = QInputDialog::getInt(Core::ICore::dialogParent(),
                                    Tr::tr("Convert Video"),
                                    Tr::tr("Enter a new width (the aspect ratio will be kept the same)"),
                                    settings->value(QStringLiteral(LAST_CONVERT_VIDEO_HRES), 320).toInt(), 16, 65535, 1, &ok,
                                    Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                    (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                        }

                        if(ok)
                        {
                            int skipFrames = ext.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive) ? QMessageBox::information(Core::ICore::dialogParent(),
                                Tr::tr("Convert Video"),
                                Tr::tr("Skip frames?"),
                                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No) : QMessageBox::No;

                            if(skipFrames != QMessageBox::Cancel)
                            {
                                bool ok = true;
                                int skip = 0;

                                if(skipFrames == QMessageBox::Yes)
                                {
                                    skip = QInputDialog::getInt(Core::ICore::dialogParent(),
                                           Tr::tr("Convert Video"),
                                           Tr::tr("Enter how many frames to skip at a time"),
                                           settings->value(QStringLiteral(LAST_CONVERT_VIDEO_SKIP), 0).toInt(), 0, 255, 1, &ok,
                                           Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                           (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                                }

                                if(ok)
                                {
                                    foreach(const QString &src, srcList)
                                    {
                                        QString tempSrc = handleImageWriterFiles(src);

                                        if(tempSrc.isEmpty())
                                        {
                                            QMessageBox::critical(Core::ICore::dialogParent(),
                                                Tr::tr("Convert Video"),
                                                Tr::tr("Unable to overwrite output file!"));

                                            return;
                                        }

                                        QString dst = QDir::cleanPath(QDir::fromNativeSeparators(dstFolder + QDir::separator() + QFileInfo(tempSrc).baseName() + ext));

                                        if((!QFile(dst).exists()) || QFile::remove(dst))
                                        {
                                            if(!convertVideoFile(dst, tempSrc, scale, skip))
                                            {
                                                QMessageBox::critical(Core::ICore::dialogParent(),
                                                    Tr::tr("Convert Video"),
                                                    Tr::tr("Unable to overwrite output file!"));

                                                return;
                                            }
                                        }
                                        else
                                        {
                                            QMessageBox::critical(Core::ICore::dialogParent(),
                                                Tr::tr("Convert Video"),
                                                Tr::tr("Unable to overwrite output file!"));

                                            return;
                                        }
                                    }

                                    settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_SRC_PATH), QFileInfo(srcList.first()).absoluteDir().path());
                                    settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_DST_FOLDER_PATH), dstFolder);
                                    settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_DST_EXTENSION), extension);
                                    if(rescale == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_HRES), scale);
                                    if(skipFrames == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_SKIP), skip);

                                    QMessageBox::information(Core::ICore::dialogParent(),
                                        Tr::tr("Convert Video"),
                                        Tr::tr("Video conversion finished!"));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else if(srcList.size() == 1)
    {
        QString dst, src = srcList.at(0);

        forever
        {
            dst =
            QFileDialog::getSaveFileName(Core::ICore::dialogParent(), Tr::tr("Convert Video Output"),
                settings->value(QStringLiteral(LAST_CONVERT_VIDEO_DST_PATH), QDir::homePath()).toString(),
                Tr::tr("Video Files (*.mp4 *.*);;OpenMV ImageReader Files (*.bin);;") + getOutputFormats());

            if((!dst.isEmpty()) && QFileInfo(dst).completeSuffix().isEmpty())
            {
                QMessageBox::warning(Core::ICore::dialogParent(),
                    Tr::tr("Convert Video Output"),
                    Tr::tr("Please add a file extension!"));

                continue;
            }

            break;
        }

        if(!dst.isEmpty())
        {
            int rescale = QMessageBox::information(Core::ICore::dialogParent(),
                Tr::tr("Convert Video"),
                Tr::tr("Rescale the video?"),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

            if(rescale != QMessageBox::Cancel)
            {
                bool ok = true;
                int scale = -1;

                if(rescale == QMessageBox::Yes)
                {
                    scale = QInputDialog::getInt(Core::ICore::dialogParent(),
                            Tr::tr("Convert Video"),
                            Tr::tr("Enter a new width (the aspect ratio will be kept the same)"),
                            settings->value(QStringLiteral(LAST_CONVERT_VIDEO_HRES), 320).toInt(), 16, 65535, 1, &ok,
                            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                }

                if(ok)
                {
                    int skipFrames = dst.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive) ? QMessageBox::information(Core::ICore::dialogParent(),
                        Tr::tr("Convert Video"),
                        Tr::tr("Skip frames?"),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No) : QMessageBox::No;

                    if(skipFrames != QMessageBox::Cancel)
                    {
                        bool ok = true;
                        int skip = 0;

                        if(skipFrames == QMessageBox::Yes)
                        {
                            skip = QInputDialog::getInt(Core::ICore::dialogParent(),
                                   Tr::tr("Convert Video"),
                                   Tr::tr("Enter how many frames to skip at a time"),
                                   settings->value(QStringLiteral(LAST_CONVERT_VIDEO_SKIP), 0).toInt(), 0, 255, 1, &ok,
                                   Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                   (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                        }

                        if(ok)
                        {
                            QString tempSrc = handleImageWriterFiles(src);

                            if((!QFile(dst).exists()) || QFile::remove(dst))
                            {
                                if((!tempSrc.isEmpty()) && convertVideoFile(dst, tempSrc, scale, skip))
                                {
                                    settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_SRC_PATH), src);
                                    settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_DST_PATH), dst);
                                    if(rescale == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_HRES), scale);
                                    if(skipFrames == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_CONVERT_VIDEO_SKIP), skip);

                                    QMessageBox::information(Core::ICore::dialogParent(),
                                        Tr::tr("Convert Video"),
                                        Tr::tr("Video conversion finished!"));
                                }
                            }
                            else
                            {
                                QMessageBox::critical(Core::ICore::dialogParent(),
                                    Tr::tr("Convert Video"),
                                    Tr::tr("Unable to overwrite output file!"));
                            }
                        }
                    }
                }
            }
        }
    }

    settings->endGroup();
}

void playVideoFileAction(const QString &drivePath)
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(VIDEO_SETTINGS_GROUP));

    QString path =
        QFileDialog::getOpenFileName(Core::ICore::dialogParent(), Tr::tr("Play Video"),
            settings->value(QStringLiteral(LAST_PLAY_VIDEO_PATH), drivePath.isEmpty() ? QDir::homePath() : drivePath).toString(),
            Tr::tr("Video Files (*.mp4 *.*);;OpenMV ImageWriter Files (*.bin);;") + getInputFormats());

    if(!path.isEmpty())
    {
        QString tempPath = handleImageWriterFiles(path);

        if((!tempPath.isEmpty()) && playVideoFile(tempPath))
        {
            settings->setValue(QStringLiteral(LAST_PLAY_VIDEO_PATH), path);
        }
    }

    settings->endGroup();
}

void playRTSPStreamAction()
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(VIDEO_SETTINGS_GROUP));

    QDialog *dialog = new QDialog(Core::ICore::dialogParent(),
        Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
    dialog->setWindowTitle(Tr::tr("Play RTSP Stream"));
    QFormLayout *layout = new QFormLayout(dialog);
    layout->setVerticalSpacing(0);

    QLabel *urlChooserTitle = new QLabel(Tr::tr("Please enter a IP address (or domain name)"));
    layout->addRow(urlChooserTitle);
    layout->addItem(new QSpacerItem(0, 6));

    QLineEdit *urlChooser = new QLineEdit(settings->value(QStringLiteral(LAST_PLAY_RTSP_URL), QStringLiteral("xxx.xxx.xxx.xxx")).toString());
    layout->addRow(urlChooser);
    layout->addItem(new QSpacerItem(0, 6));

    QLabel *portChooserTitle = new QLabel(Tr::tr("Please enter a Port (the RTSP default port is 554)"));
    layout->addRow(portChooserTitle);
    layout->addItem(new QSpacerItem(0, 6));

    QLineEdit *portChooser = new QLineEdit(settings->value(QStringLiteral(LAST_PLAY_RTSP_PORT), QStringLiteral("554")).toString());
    layout->addRow(portChooser);
    layout->addItem(new QSpacerItem(0, 6));

    QHBoxLayout *layout2 = new QHBoxLayout;
    layout2->setContentsMargins(0, 0, 0, 0);
    QWidget *widget = new QWidget;
    widget->setLayout(layout2);

    QCheckBox *checkBox = new QCheckBox(Tr::tr("Stream video over TCP (versus UDP)?"));
    checkBox->setChecked(settings->value(QStringLiteral(LAST_PLAY_RTSP_TCP), false).toBool());
    layout2->addWidget(checkBox);
    checkBox->setToolTip(Tr::tr("Keeps the RTP video stream inside of the same TCP socket used for setting up the initial connection "
                                     "verus creating a new UDP video stream. This may help the connection on networks with firewalls."));

    QUrl u(QStringLiteral("rtsp://") + urlChooser->text() + QLatin1Char(':') + QString::number(portChooser->text().toInt()));

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QPushButton *run = new QPushButton(Tr::tr("Play"));
    run->setEnabled(u.isValid());
    box->addButton(run, QDialogButtonBox::AcceptRole);
    layout2->addSpacing(160);
    layout2->addWidget(box);
    layout->addRow(widget);

    QObject::connect(box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    QObject::connect(urlChooser, &QLineEdit::textChanged, [portChooser, run] (const QString &text) {
        QUrl u(QStringLiteral("rtsp://") + text + QLatin1Char(':') + QString::number(portChooser->text().toInt()));
        run->setEnabled(u.isValid());
    });

    QObject::connect(portChooser, &QLineEdit::textChanged, [urlChooser, run] (const QString &text) {
        QUrl u(QStringLiteral("rtsp://") + urlChooser->text() + QLatin1Char(':') + QString::number(text.toInt()));
        run->setEnabled(u.isValid());
    });

    if(dialog->exec() == QDialog::Accepted)
    {
        QString url = urlChooser->text();
        QString port = portChooser->text();

        QUrl u(QStringLiteral("rtsp://") + url + QLatin1Char(':') + QString::number(port.toInt()));

        if(playRTSPStream(u, checkBox->isChecked()))
        {
            settings->setValue(QStringLiteral(LAST_PLAY_RTSP_URL), url);
            settings->setValue(QStringLiteral(LAST_PLAY_RTSP_PORT), port);
            settings->setValue(QStringLiteral(LAST_PLAY_RTSP_TCP), checkBox->isChecked());
        }
    }

    settings->endGroup();
    delete dialog;
}

void saveVideoFile(const QString &srcPath)
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(VIDEO_SETTINGS_GROUP));

    QString dst;

    forever
    {
        dst =
        QFileDialog::getSaveFileName(Core::ICore::dialogParent(), Tr::tr("Save Video"),
            settings->value(QStringLiteral(LAST_SAVE_VIDEO_PATH), QDir::homePath()).toString(),
            Tr::tr("Video Files (*.mp4 *.*);;OpenMV ImageReader Files (*.bin);;") + getOutputFormats());

        if((!dst.isEmpty()) && QFileInfo(dst).completeSuffix().isEmpty())
        {
            QMessageBox::warning(Core::ICore::dialogParent(),
                Tr::tr("Save Video"),
                Tr::tr("Please add a file extension!"));

            continue;
        }

        break;
    }

    if(!dst.isEmpty())
    {
        int rescale = QMessageBox::information(Core::ICore::dialogParent(),
            Tr::tr("Convert Video"),
            Tr::tr("Rescale the video?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

        if(rescale != QMessageBox::Cancel)
        {
            bool ok = true;
            int scale = -1;

            if(rescale == QMessageBox::Yes)
            {
                scale = QInputDialog::getInt(Core::ICore::dialogParent(),
                        Tr::tr("Convert Video"),
                        Tr::tr("Enter a new width (the aspect ratio will be kept the same)"),
                        settings->value(QStringLiteral(LAST_SAVE_VIDEO_HRES), 320).toInt(), 16, 65535, 1, &ok,
                        Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
            }

            if(ok)
            {
                int skipFrames = dst.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive) ? QMessageBox::information(Core::ICore::dialogParent(),
                    Tr::tr("Convert Video"),
                    Tr::tr("Skip frames?"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No) : QMessageBox::No;

                if(skipFrames != QMessageBox::Cancel)
                {
                    bool ok = true;
                    int skip = 0;

                    if(skipFrames == QMessageBox::Yes)
                    {
                        skip = QInputDialog::getInt(Core::ICore::dialogParent(),
                               Tr::tr("Convert Video"),
                               Tr::tr("Enter how many frames to skip at a time"),
                               settings->value(QStringLiteral(LAST_SAVE_VIDEO_SKIP), 0).toInt(), 0, 255, 1, &ok,
                               Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                               (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                    }

                    if(ok)
                    {
                        QString tempSrc = handleImageWriterFiles(srcPath);

                        if((!QFile(dst).exists()) || QFile::remove(dst))
                        {
                            if((!tempSrc.isEmpty()) && convertVideoFile(dst, tempSrc, scale, skip))
                            {
                                settings->setValue(QStringLiteral(LAST_SAVE_VIDEO_PATH), dst);
                                if(rescale == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_SAVE_VIDEO_HRES), scale);
                                if(skipFrames == QMessageBox::Yes) settings->setValue(QStringLiteral(LAST_SAVE_VIDEO_SKIP), skip);

                                QMessageBox::information(Core::ICore::dialogParent(),
                                    Tr::tr("Convert Video"),
                                    Tr::tr("Video conversion finished!"));
                            }
                        }
                        else
                        {
                            QMessageBox::critical(Core::ICore::dialogParent(),
                                Tr::tr("Convert Video"),
                                Tr::tr("Unable to overwrite output file!"));
                        }
                    }
                }
            }
        }
    }

    settings->endGroup();
}

} // namespace Internal
} // namespace OpenMV
