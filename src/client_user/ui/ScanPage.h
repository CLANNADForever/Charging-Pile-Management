#pragma once

#include <QString>

#include "Page.h"

class QCamera;
class QMediaCaptureSession;
class QImageCapture;
class QVideoWidget;
class QLabel;
class QPushButton;

// 扫码充电页:采用老师的 QImageCapture 拍照方案——QVideoWidget 实时预览,
// 点「识别」拍一帧解码二维码(避免反复拍照的内存问题)。
class ScanPage : public Page
{
    Q_OBJECT
public:
    explicit ScanPage(QWidget *parent = nullptr);
    ~ScanPage() override;

signals:
    void scanned(const QString &text);

private:
    void startCamera();
    void stopCamera();
    void onShutterClicked();
    void onImageCaptured(int id, const QImage &image);
    void onImageFileClicked();
    void tryDecode(const QImage &image);

    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_session = nullptr;
    QImageCapture *m_imageCapture = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_shutterBtn = nullptr;
    bool m_decoded = false;
};
