#include "V4l2Camera.h"

#include <QDebug>
#include <QTimer>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

// 从缓冲里截取 JPEG(SOI 0xFFD8 ... EOI 0xFFD9),容忍前后有填充字节
QImage decodeJpeg(const uchar *data, size_t len)
{
    if (len < 4)
        return QImage();

    // 找 SOI
    size_t soi = 0;
    for (size_t i = 0; i + 1 < len; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD8) {
            soi = i;
            break;
        }
    }
    // 找 EOI(从后往前)
    size_t eoi = len;
    for (size_t i = len - 1; i > soi + 1; --i) {
        if (data[i - 1] == 0xFF && data[i] == 0xD9) {
            eoi = i + 1;
            break;
        }
    }
    if (eoi <= soi)
        return QImage();

    return QImage::fromData(data + soi, int(eoi - soi), "JPG");
}

} // namespace

V4l2Camera::V4l2Camera(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(500); // ~2fps,给渲染留足时间彻底消除撕裂
    connect(m_timer, &QTimer::timeout, this, &V4l2Camera::pollFrame);
}

V4l2Camera::~V4l2Camera()
{
    stop();
}

bool V4l2Camera::open(const QString &devicePath, int width, int height)
{
    m_fd = ::open(devicePath.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        qDebug() << "[扫码] open 失败 errno=" << errno << strerror(errno);
        return false;
    }

    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (::ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        qDebug() << "[扫码] S_FMT 失败 errno=" << errno << strerror(errno);
        stop();
        return false;
    }
    m_width = int(fmt.fmt.pix.width);
    m_height = int(fmt.fmt.pix.height);
    qDebug() << "[扫码] S_FMT 成功 分辨率=" << m_width << "x" << m_height
             << " 像素格式(4cc)=" << fmt.fmt.pix.pixelformat;

    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (::ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        qDebug() << "[扫码] REQBUFS 失败 errno=" << errno << strerror(errno);
        stop();
        return false;
    }

    m_buffers.resize(req.count);
    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (::ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qDebug() << "[扫码] QUERYBUF" << i << "失败 errno=" << errno;
            stop();
            return false;
        }
        m_buffers[i].length = buf.length;
        m_buffers[i].start = ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, m_fd, buf.m.offset);
        if (m_buffers[i].start == MAP_FAILED) {
            qDebug() << "[扫码] mmap" << i << "失败";
            stop();
            return false;
        }
    }

    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (::ioctl(m_fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "[扫码] QBUF" << i << "失败 errno=" << errno;
            stop();
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        qDebug() << "[扫码] STREAMON 失败 errno=" << errno << strerror(errno);
        stop();
        return false;
    }
    qDebug() << "[扫码] STREAMON 成功";
    return true;
}

void V4l2Camera::start()
{
    if (m_timer)
        m_timer->start();
}

void V4l2Camera::stop()
{
    if (m_timer)
        m_timer->stop();
    if (m_fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ::ioctl(m_fd, VIDIOC_STREAMOFF, &type);
        for (Buffer &b : m_buffers) {
            if (b.start)
                ::munmap(b.start, b.length);
        }
        m_buffers.clear();
        ::close(m_fd);
        m_fd = -1;
    }
}

void V4l2Camera::pollFrame()
{
    if (m_fd < 0)
        return;

    struct v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0)
        return; // EAGAIN

    const uchar *data = reinterpret_cast<const uchar *>(m_buffers[buf.index].start);
    const size_t len = buf.bytesused;

    if (m_frameCount < 3) {
        qDebug() << "[扫码] 帧 len=" << len
                 << " 前4字节=" << Qt::hex << data[0] << data[1] << data[2] << data[3] << Qt::dec;
    }

    QImage img = decodeJpeg(data, len);
    if (img.isNull()) {
        if (m_frameCount < 3)
            qDebug() << "[扫码] JPEG 解码失败";
    } else {
        if (m_frameCount < 3)
            qDebug() << "[扫码] JPEG 解码成功 尺寸=" << img.size();
        emit frameReady(img);
    }
    ++m_frameCount;

    ::ioctl(m_fd, VIDIOC_QBUF, &buf);
}
