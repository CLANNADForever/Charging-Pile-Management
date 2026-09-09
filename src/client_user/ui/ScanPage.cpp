#include "ScanPage.h"

#include <QCamera>
#include <QFileDialog>
#include <QImage>
#include <QImageCapture>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <cstring>

#include "BarcodeFormat.h"
#include "DecodeHints.h"
#include "ImageView.h"
#include "ReadBarcode.h"
#include "Result.h"
#include "common/Toast.h"

ScanPage::ScanPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(makeHeader(QStringLiteral("扫码充电")));

    auto *body = new QVBoxLayout;
    body->setContentsMargins(20, 20, 20, 20);
    body->setSpacing(12);

    // 实时预览(老师方案)
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMinimumHeight(280);
    m_videoWidget->setObjectName(QStringLiteral("scanPreview"));
    body->addWidget(m_videoWidget, 1);

    m_statusLabel = new QLabel(QStringLiteral("将二维码对准画面点击识别，或从图片识别"), this);
    m_statusLabel->setObjectName(QStringLiteral("hintLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    body->addWidget(m_statusLabel);

    m_shutterBtn = new QPushButton(QStringLiteral("拍照识别"), this);
    m_shutterBtn->setObjectName(QStringLiteral("primaryButton"));
    m_shutterBtn->setCursor(Qt::PointingHandCursor);
    connect(m_shutterBtn, &QPushButton::clicked, this, &ScanPage::onShutterClicked);
    body->addWidget(m_shutterBtn);

    auto *fileBtn = new QPushButton(QStringLiteral("从图片识别"), this);
    fileBtn->setObjectName(QStringLiteral("ghostButton"));
    fileBtn->setCursor(Qt::PointingHandCursor);
    connect(fileBtn, &QPushButton::clicked, this, &ScanPage::onImageFileClicked);
    body->addWidget(fileBtn);

    root->addLayout(body, 1);

    startCamera();
}

ScanPage::~ScanPage()
{
    stopCamera();
}

void ScanPage::startCamera()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("未检测到摄像头（可用「从图片识别」）"));
        m_shutterBtn->setEnabled(false);
        return;
    }

    m_session = new QMediaCaptureSession(this);
    m_imageCapture = new QImageCapture(this);
    m_camera = new QCamera(cameras.first(), this);

    m_session->setCamera(m_camera);
    m_session->setVideoOutput(m_videoWidget);
    m_session->setImageCapture(m_imageCapture);

    connect(m_imageCapture, &QImageCapture::imageCaptured, this,
            &ScanPage::onImageCaptured);

    m_camera->start();
}

void ScanPage::stopCamera()
{
    if (m_camera && m_camera->isActive())
        m_camera->stop();
}

void ScanPage::onShutterClicked()
{
    if (m_decoded || !m_imageCapture || !m_imageCapture->isReadyForCapture())
        return;
    m_imageCapture->capture();
}

void ScanPage::onImageCaptured(int, const QImage &image)
{
    tryDecode(image);
}

void ScanPage::onImageFileClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择二维码图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;
    QImage image(path);
    if (image.isNull()) {
        m_statusLabel->setText(QStringLiteral("无法读取图片，请换一张"));
        return;
    }
    tryDecode(image);
}

void ScanPage::tryDecode(const QImage &image)
{
    if (m_decoded || image.isNull())
        return;

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull())
        return;

    // 手动构造连续灰度缓冲,避免行填充让 ZXing 越界
    const int w = gray.width();
    const int h = gray.height();
    QByteArray lum(w * h, 0);
    for (int y = 0; y < h; ++y)
        std::memcpy(lum.data() + size_t(y) * w, gray.constScanLine(y), size_t(w));

    ZXing::ImageView view(reinterpret_cast<const uint8_t *>(lum.constData()), w, h,
                          ZXing::ImageFormat::Lum);
    const ZXing::Result result =
        ZXing::ReadBarcode(view, ZXing::DecodeHints().setFormats(ZXing::BarcodeFormat::QRCode));

    if (result.isValid()) {
        m_decoded = true;
        stopCamera();
        const QString text = QString::fromStdString(result.text());
        m_statusLabel->setText(QStringLiteral("识别成功：") + text);
        emit scanned(text);
    } else {
        m_statusLabel->setText(QStringLiteral("未识别到二维码，请对准后重试"));
    }
}
