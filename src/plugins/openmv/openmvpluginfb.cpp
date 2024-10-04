#include "openmvpluginfb.h"

#include <coreplugin/icore.h>
#include <utils/theme/theme.h>

#include "openmvpluginio.h"
#include "tools/videotools.h"

#include "openmvtr.h"

namespace OpenMV {
namespace Internal {

OpenMVPluginFB::OpenMVPluginFB(QWidget *parent) : QGraphicsView(parent), m_enableSaveTemplate(false), m_enableSaveDescriptor(false), m_enableInteraction(true)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);
    setMinimumWidth(160);
    setMinimumHeight(120);
    setBackgroundBrush(Utils::creatorTheme()->color(Utils::Theme::BackgroundColorNormal));
    setScene(new QGraphicsScene(this));

    QGraphicsTextItem *item = new QGraphicsTextItem;
    item->setHtml(QString(QStringLiteral("<html><body style=\"color:%1;font-size:14px\">"
    "<div align=\"center\">"
    "<div style=\"font-size:20px\">%2</div>"
    "</div>"
    "</body></html>")).arg(Utils::creatorTheme()->color(Utils::Theme::TextColorDisabled).name()).arg(Tr::tr("No Image")));
    scene()->addItem(item);

    m_enableFitInView = false;
    m_pixmap = Q_NULLPTR;
    m_unlocked = false;
    m_origin = QPoint();

    m_band = new QRubberBand(QRubberBand::Rectangle, this);
    m_band->setGeometry(QRect());
    m_band->hide();

    m_timer = new QTimer(this);
    m_tempFile = Q_NULLPTR;
    m_elaspedTimer = QElapsedTimer();
    m_previousElaspedTimers = QQueue<qint64>();

    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &OpenMVPluginFB::private_timerCallBack);
}

bool OpenMVPluginFB::pixmapValid() const
{
    return m_pixmap;
}

QPixmap OpenMVPluginFB::pixmap() const
{
    return m_pixmap ? m_pixmap->pixmap() : QPixmap();
}

bool OpenMVPluginFB::beginImageWriter()
{
    m_tempFile = new QTemporaryFile(this);

    if(m_tempFile->open())
    {
        if(m_tempFile->write("OMV IMG STR V1.0") == 16)
        {
            m_timer->start(1000 / VIDEO_RECORDER_FRAME_RATE);
            m_elaspedTimer.start();

            return true;
        }
        else
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                Tr::tr("Video Record"),
                Tr::tr("Error: %L1!").arg(m_tempFile->errorString()));
        }
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Video Record"),
            Tr::tr("Error: %L1!").arg(m_tempFile->errorString()));
    }

    delete m_tempFile;
    m_tempFile = Q_NULLPTR;

    return false;
}

void OpenMVPluginFB::endImageWriter()
{
    m_timer->stop();
    m_tempFile->close();
    m_elaspedTimer = QElapsedTimer();
    m_previousElaspedTimers = QQueue<qint64>();

    saveVideoFile(QFileInfo(*m_tempFile).canonicalFilePath());

    delete m_tempFile;
    m_tempFile = Q_NULLPTR;
}

void OpenMVPluginFB::enableFitInView(bool enable)
{
    m_enableFitInView = enable;

    myFitInView(m_pixmap);

    if(m_band->isVisible())
    {
        m_unlocked = false;
        m_band->setGeometry(QRect());
        m_band->hide();

        broadcastUpdate();
    }
}

void OpenMVPluginFB::frameBufferData(const QPixmap &data)
{
    delete scene();
    setScene(new QGraphicsScene(this));

    if(!data.isNull())
    {
        m_pixmap = scene()->addPixmap(data);
    }
    else
    {
        m_pixmap = Q_NULLPTR;
        QGraphicsTextItem *item = new QGraphicsTextItem;
        item->setHtml(QString(QStringLiteral("<html><body style=\"color:%1;font-size:14px\">"
        "<div align=\"center\">"
        "<div style=\"font-size:20px\">%2</div>"
        "</div>"
        "</body></html>")).arg(Utils::creatorTheme()->color(Utils::Theme::TextColorDisabled).name()).arg(Tr::tr("No Image")));
        scene()->addItem(item);
    }

    myFitInView(m_pixmap);

    broadcastUpdate();
}

void OpenMVPluginFB::private_timerCallBack()
{
    m_previousElaspedTimers.push_back(m_elaspedTimer.elapsed());

    if(m_previousElaspedTimers.size() > 11)
    {
        m_previousElaspedTimers.pop_front();
    }

    qint64 average = 0;

    for(int i = 1; i < m_previousElaspedTimers.size(); i++)
    {
        average += m_previousElaspedTimers.at(i) - m_previousElaspedTimers.at(i - 1);
    }

    average /= m_previousElaspedTimers.size();

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly); // always return true
    m_pixmap->pixmap().save(&buffer, "JPG"); // always return true
    buffer.close();

    QByteArray data;
    serializeLong(data, static_cast<int>(m_previousElaspedTimers.last() - ((m_previousElaspedTimers.size() > 1) ? m_previousElaspedTimers.at(m_previousElaspedTimers.size() - 2) : 0)));
    serializeLong(data, m_pixmap->pixmap().width());
    serializeLong(data, m_pixmap->pixmap().height());
    serializeLong(data, jpeg.size());
    data.append(jpeg);

    int size = 16 - (jpeg.size() % 16);
    if(size != 16) data.append(QByteArray(size, 0));

    if(m_tempFile->write(data) == data.size())
    {
        qint64 milliseconds = m_previousElaspedTimers.last() % 1000;
        qint64 seconds = (m_previousElaspedTimers.last() / 1000) % 60;
        qint64 minutes = ((m_previousElaspedTimers.last() / 1000) / 60) % 60;
        qint64 hours = ((m_previousElaspedTimers.last() / 1000) / 60) / 60;

        int kilobyte = 1024;
        int megabyte = kilobyte * kilobyte;
        int gigabyte = kilobyte * kilobyte * kilobyte;

        if(m_tempFile->size() < kilobyte)
        {
            emit imageWriterTick(Tr::tr("Elapsed: ") +
                                 QString(QStringLiteral("%1h:")).arg(hours) +
                                 QString(QStringLiteral("%1m:")).arg(minutes, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1s:")).arg(seconds, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1ms")).arg(milliseconds, 3, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral(" - Size: %1 B")).arg(m_tempFile->size()) +
                                 QString(QStringLiteral(" - FPS: %1")).arg(average ? (1000 / double(average)) : 0, 5, 'f', 1));
        }
        else if(m_tempFile->size() < megabyte)
        {
            emit imageWriterTick(Tr::tr("Elapsed: ") +
                                 QString(QStringLiteral("%1h:")).arg(hours) +
                                 QString(QStringLiteral("%1m:")).arg(minutes, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1s:")).arg(seconds, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1ms")).arg(milliseconds, 3, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral(" - Size: %1 KB")).arg(m_tempFile->size() / double(kilobyte), 5, 'f', 3) +
                                 QString(QStringLiteral(" - FPS: %1")).arg(average ? (1000 / double(average)) : 0, 5, 'f', 1));
        }
        else if(m_tempFile->size() < gigabyte)
        {
            emit imageWriterTick(Tr::tr("Elapsed: ") +
                                 QString(QStringLiteral("%1h:")).arg(hours) +
                                 QString(QStringLiteral("%1m:")).arg(minutes, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1s:")).arg(seconds, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1ms")).arg(milliseconds, 3, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral(" - Size: %1 MB")).arg(m_tempFile->size() / double(megabyte), 5, 'f', 3) +
                                 QString(QStringLiteral(" - FPS: %1")).arg(average ? (1000 / double(average)) : 0, 5, 'f', 1));
        }
        else
        {
            emit imageWriterTick(Tr::tr("Elapsed: ") +
                                 QString(QStringLiteral("%1h:")).arg(hours) +
                                 QString(QStringLiteral("%1m:")).arg(minutes, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1s:")).arg(seconds, 2, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral("%1ms")).arg(milliseconds, 3, 10, QLatin1Char('0')) +
                                 QString(QStringLiteral(" - Size: %1 GB")).arg(m_tempFile->size() / double(gigabyte), 5, 'f', 3) +
                                 QString(QStringLiteral(" - FPS: %1")).arg(average ? (1000 / double(average)) : 0, 5, 'f', 1));
        }
    }
    else
    {
        m_timer->stop();
        m_tempFile->close();
        m_elaspedTimer = QElapsedTimer();
        m_previousElaspedTimers = QQueue<qint64>();

        QMessageBox::critical(Core::ICore::dialogParent(),
            Tr::tr("Video Recorder"),
            Tr::tr("Failed to write frame!"));

        emit imageWriterShutdown();

        delete m_tempFile;
        m_tempFile = Q_NULLPTR;
    }
}

void OpenMVPluginFB::mousePressEvent(QMouseEvent *event)
{
    if(m_enableInteraction && (event->button() == Qt::LeftButton))
    {
        m_unlocked = m_pixmap && (m_pixmap == itemAt(event->pos()));
        m_origin = event->pos();
        m_band->setGeometry(QRect(m_origin, event->pos() + QPoint(1, 1)).normalized().intersected(mapFromScene(sceneRect()).boundingRect()));
        m_band->setVisible(m_pixmap && getROI().isValid());

        broadcastUpdate();
    }

    QGraphicsView::mousePressEvent(event);
}

void OpenMVPluginFB::mouseMoveEvent(QMouseEvent *event)
{
    if(m_unlocked)
    {
        m_band->setGeometry(QRect(m_origin, event->pos()).normalized().intersected(mapFromScene(sceneRect()).boundingRect()));
        m_band->show();

        broadcastUpdate();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void OpenMVPluginFB::mouseReleaseEvent(QMouseEvent *event)
{
    m_unlocked = false;

    QGraphicsView::mouseReleaseEvent(event);
}

void OpenMVPluginFB::contextMenuEvent(QContextMenuEvent *event)
{
    if(m_enableInteraction && m_pixmap && (m_pixmap == itemAt(event->pos())))
    {
        bool cropped;
        QRect croppedRect;
        QPixmap pixmap = getPixmap(true, event->pos(), &cropped, &croppedRect);

        QMenu menu(this);

        QAction *sImage = menu.addAction(cropped ? Tr::tr("Save Image selection to PC") : Tr::tr("Save Image to PC"));
        menu.addSeparator();
        QAction *sTemplate = menu.addAction(cropped ? Tr::tr("Save Template selection to Cam") : Tr::tr("Save Template to Cam"));
        sTemplate->setVisible(m_enableSaveTemplate);
        // Disable Save TEMPLATE
        sTemplate->setVisible(false);
        // Disable Save TEMPLATE
        QAction *sDescriptor = menu.addAction(cropped ? Tr::tr("Save Descriptor selection to Cam") : Tr::tr("Save Descriptor to Cam"));
        sDescriptor->setVisible(m_enableSaveDescriptor);
        // Disable Save Descriptor
        sDescriptor->setVisible(false);
        // Disable Save Descriptor

        QAction *selected = menu.exec(event->globalPos());

        if(selected == sImage)
        {
            emit saveImage(pixmap);
        }
        else if(selected == sTemplate)
        {
            emit saveTemplate(croppedRect);
        }
        else if(selected == sDescriptor)
        {
            emit saveDescriptor(croppedRect);
        }
    }
}

void OpenMVPluginFB::resizeEvent(QResizeEvent *event)
{
    myFitInView(m_pixmap);

    if(m_band->isVisible())
    {
        m_unlocked = false;
        m_band->setGeometry(QRect());
        m_band->hide();

        broadcastUpdate();
    }

    QGraphicsView::resizeEvent(event);
}

QRect OpenMVPluginFB::getROI()
{
    return m_band->geometry().isValid() ? m_pixmap->mapFromScene(mapToScene(m_band->geometry())).boundingRect().toRect() : QRect();
}

QPixmap OpenMVPluginFB::getPixmap(bool pointValid, const QPoint &point, bool *cropped, QRect *croppedRect)
{
    if(!m_pixmap)
    {
        return QPixmap();
    }

    bool crop = m_band->geometry().isValid() && ((!pointValid) || m_band->geometry().contains(point));

    if(cropped)
    {
        *cropped = crop;
    }

    QRect rect = getROI();

    if(croppedRect)
    {
        *croppedRect = crop ? rect : m_pixmap->pixmap().rect();
    }

    if(crop)
    {
        return m_pixmap->pixmap().copy(rect);
    }

    return m_pixmap->pixmap();
}

void OpenMVPluginFB::myFitInView(QGraphicsPixmapItem *item)
{
    qreal scale = qMin(width() / sceneRect().width(), height() / sceneRect().height());
    QTransform matrix(1, 0, 0, 0, 1, 0, 0, 0, 1);

    if(item && m_enableFitInView)
    {
        matrix.scale(scale, scale);
    }

    setTransform(matrix);

    if(item) centerOn(item);
}

int start_of_scan_offset(const QByteArray &jpeg) {
    for (uint8_t *pstart = ((uint8_t *) jpeg.data()), *p = pstart, *pend = pstart + jpeg.length(); p < pend; ) {
        uint16_t header = (p[0] << 8) | p[1];
        p += sizeof(uint16_t);

        if ((0xFFD0 <= header) && (header <= 0xFFD9)) {
            continue;
        } else if (0xFFDA == header) {
            // Start-of-Scan (no more jpeg headers left).
            return p - pstart;
        } else if (((0xFFC0 <= header) && (header <= 0xFFCF))
                   || ((0xFFDB <= header) && (header <= 0xFFDF))
                   || ((0xFFE0 <= header) && (header <= 0xFFEF))
                   || ((0xFFF0 <= header) && (header <= 0xFFFE))) {
            uint16_t size = (p[0] << 8) | p[1];
            p += sizeof(uint16_t);

            if (((0xFFC1 <= header) && (header <= 0xFFC3))
                || ((0xFFC5 <= header) && (header <= 0xFFC7))
                || ((0xFFC9 <= header) && (header <= 0xFFCB))
                || ((0xFFCD <= header) && (header <= 0xFFCF))) {
                // Non-baseline jpeg.
                return 0;
            } else {
                p += size - sizeof(uint16_t);
            }
        } else {
            // Invalid JPEG
            return 0;
        }
    }

    return 0;
}

void OpenMVPluginFB::broadcastUpdate()
{
    // Broadcast the new pixmap
    emit pixmapUpdate(getPixmap());

    if(m_pixmap)
    {
        QRect rect = getROI();

        QBuffer buffer;
        QImageWriter writer(&buffer, "jpg");
        QPixmap temp = m_pixmap->pixmap();//.copy(rect); // new
        writer.write(temp.toImage());
        int focusMetric = ((buffer.size() - start_of_scan_offset(buffer.data())) * 100 * 8) /
                          (temp.rect().width() * temp.rect().height());
        temp = QPixmap(); // free

        emit resolutionAndROIUpdate(m_pixmap->pixmap().size(), rect, focusMetric);
    }
    else
    {
        emit resolutionAndROIUpdate(QSize(), QRect(), 0);
    }
}

} // namespace Internal
} // namespace OpenMV
