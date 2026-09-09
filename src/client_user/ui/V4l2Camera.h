#pragma once

#include <QImage>
#include <QObject>
#include <QVector>

class QTimer;

// 直接通过 V4L2 mmap 流式采集读摄像头帧(绕过 Qt Multimedia 的 GStreamer 后端)。
// MJPEG 格式 + VIDIOC_STREAMON 触发采集(点亮摄像头 LED)。
class V4l2Camera : public QObject
{
    Q_OBJECT
public:
    explicit V4l2Camera(QObject *parent = nullptr);
    ~V4l2Camera() override;

    bool open(const QString &devicePath, int width = 640, int height = 480);
    void start();
    void stop();
    bool isOpen() const { return m_fd >= 0; }

signals:
    void frameReady(const QImage &image);

private:
    struct Buffer {
        void *start = nullptr;
        size_t length = 0;
    };

    void pollFrame();

    int m_fd = -1;
    int m_width = 0;
    int m_height = 0;
    int m_dqbufFailCount = 0;
    int m_frameCount = 0;
    QTimer *m_timer = nullptr;
    QVector<Buffer> m_buffers;
};
