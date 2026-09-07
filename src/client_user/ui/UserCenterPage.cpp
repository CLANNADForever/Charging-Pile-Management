#include "UserCenterPage.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/Toast.h"
#include "common/Utils.h"
#include "core/service/UserService.h"

namespace {

QPixmap circleClip(const QPixmap &src, int size)
{
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    p.drawPixmap(0, 0, size, size,
                 src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    return result;
}

QPixmap defaultAvatar(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xE1, 0xE4, 0xEA));
    p.drawEllipse(0, 0, size, size);
    p.setBrush(QColor(0xFF, 0xFF, 0xFF));
    p.drawEllipse(QRectF(size * 0.30, size * 0.22, size * 0.40, size * 0.40));
    QPainterPath body;
    body.addRoundedRect(QRectF(size * 0.15, size * 0.62, size * 0.70, size * 0.30),
                        size * 0.15, size * 0.15);
    p.drawPath(body);
    return pm;
}

} // namespace

UserCenterPage::UserCenterPage(QWidget *parent)
    : Page(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 24, 20, 20);
    root->setSpacing(0);

    // ===== 头像 + 昵称 =====
    auto *header = new QHBoxLayout;
    header->setSpacing(16);

    m_avatarBtn = new QPushButton(this);
    m_avatarBtn->setFixedSize(72, 72);
    m_avatarBtn->setIconSize(QSize(72, 72));
    m_avatarBtn->setFlat(true);
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    m_avatarBtn->setToolTip(QStringLiteral("点击更换头像"));
    m_avatarBtn->setStyleSheet(QStringLiteral("border:none; border-radius:36px;"));
    connect(m_avatarBtn, &QPushButton::clicked, this, &UserCenterPage::onChangeAvatar);
    header->addWidget(m_avatarBtn, 0, Qt::AlignVCenter);

    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(6);
    m_nicknameBtn = new QPushButton(this);
    m_nicknameBtn->setFlat(true);
    m_nicknameBtn->setCursor(Qt::PointingHandCursor);
    m_nicknameBtn->setToolTip(QStringLiteral("点击修改昵称"));
    m_nicknameBtn->setStyleSheet(QStringLiteral(
        "border:none; text-align:left; font-size:18px; font-weight:bold; color:#1A1D26; padding:0;"));
    connect(m_nicknameBtn, &QPushButton::clicked, this, &UserCenterPage::onEditNickname);
    m_phoneLabel = new QLabel(this);
    m_phoneLabel->setObjectName(QStringLiteral("hintLabel"));
    nameCol->addWidget(m_nicknameBtn);
    nameCol->addWidget(m_phoneLabel);
    nameCol->addStretch();
    header->addLayout(nameCol, 1);

    root->addLayout(header);
    root->addSpacing(24);

    // ===== 入口卡片 =====
    auto *entryCard = new QFrame(this);
    entryCard->setObjectName(QStringLiteral("card"));
    auto *entryLay = new QVBoxLayout(entryCard);
    entryLay->setContentsMargins(16, 0, 16, 0);
    entryLay->setSpacing(0);

    auto *couponBtn = new QPushButton(QStringLiteral("优惠券管理 ›"), entryCard);
    couponBtn->setObjectName(QStringLiteral("entryButton"));
    couponBtn->setCursor(Qt::PointingHandCursor);
    connect(couponBtn, &QPushButton::clicked, this, &UserCenterPage::openCoupons);

    auto *divider = new QFrame(entryCard);
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFixedHeight(1);

    auto *orderBtn = new QPushButton(QStringLiteral("订单管理 ›"), entryCard);
    orderBtn->setObjectName(QStringLiteral("entryButton"));
    orderBtn->setCursor(Qt::PointingHandCursor);
    connect(orderBtn, &QPushButton::clicked, this, &UserCenterPage::openOrders);

    entryLay->addWidget(couponBtn);
    entryLay->addWidget(divider);
    entryLay->addWidget(orderBtn);
    root->addWidget(entryCard);

    root->addSpacing(16);

    // ===== 我的钱包卡片 =====
    auto *walletCard = new QFrame(this);
    walletCard->setObjectName(QStringLiteral("card"));
    auto *walletLay = new QVBoxLayout(walletCard);
    walletLay->setContentsMargins(16, 16, 16, 16);
    walletLay->setSpacing(12);

    auto *walletTitle = new QLabel(QStringLiteral("我的钱包"), walletCard);
    walletTitle->setObjectName(QStringLiteral("sectionTitle"));

    m_balanceLabel = new QLabel(walletCard);
    m_balanceLabel->setObjectName(QStringLiteral("balanceBig"));

    auto *rechargeRow = new QHBoxLayout;
    rechargeRow->setSpacing(10);
    m_rechargeEdit = new QLineEdit(walletCard);
    m_rechargeEdit->setPlaceholderText(QStringLiteral("充值金额(0.01~10000)"));
    auto *rechargeBtn = new QPushButton(QStringLiteral("充值"), walletCard);
    rechargeBtn->setObjectName(QStringLiteral("primaryButton"));
    rechargeBtn->setFixedWidth(96);
    connect(rechargeBtn, &QPushButton::clicked, this, &UserCenterPage::onRecharge);
    rechargeRow->addWidget(m_rechargeEdit, 1);
    rechargeRow->addWidget(rechargeBtn);

    walletLay->addWidget(walletTitle);
    walletLay->addWidget(m_balanceLabel);
    walletLay->addLayout(rechargeRow);
    root->addWidget(walletCard);

    root->addStretch(1);

    // ===== 退出登录 =====
    auto *logoutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    logoutBtn->setObjectName(QStringLiteral("dangerTextButton"));
    logoutBtn->setCursor(Qt::PointingHandCursor);
    connect(logoutBtn, &QPushButton::clicked, this, &UserCenterPage::logoutRequested);
    root->addWidget(logoutBtn);

    refresh();
}

void UserCenterPage::refresh()
{
    const User &u = UserService::instance().current();
    m_nicknameBtn->setText(u.nickname);
    m_phoneLabel->setText(Utils::maskPhone(u.phone));
    m_balanceLabel->setText(QStringLiteral("¥ ") + Utils::formatMoney(u.balance));
    m_avatarBtn->setIcon(u.avatarPath.isEmpty()
                             ? QIcon(defaultAvatar(72))
                             : QIcon(circleClip(QPixmap(u.avatarPath), 72)));
}

void UserCenterPage::onChangeAvatar()
{
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择头像"), QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp)"));
    if (file.isEmpty())
        return;

    const QFileInfo info(file);
    if (info.size() > 5 * 1024 * 1024) {
        Toast::show(this, QStringLiteral("图片过大，请选择 5MB 以内的图片"));
        return;
    }

    const QImage img(file);
    if (img.isNull()) {
        Toast::show(this, QStringLiteral("图片加载失败"));
        return;
    }

    QString err;
    if (!UserService::instance().uploadAvatar(file, &err)) {
        Toast::show(this, err);
        return;
    }
    refresh();
}

void UserCenterPage::onEditNickname()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, QStringLiteral("修改昵称"),
        QStringLiteral("昵称(1-20 字符):"), QLineEdit::Normal, m_nicknameBtn->text(), &ok);
    if (!ok)
        return;

    text = text.trimmed();
    if (text.isEmpty()) {
        Toast::show(this, QStringLiteral("昵称不能为空"));
        return;
    }
    if (text.length() > 20)
        text = text.left(20);

    QString err;
    if (!UserService::instance().updateNickname(text, &err)) {
        Toast::show(this, err);
        return;
    }
    refresh();
}

void UserCenterPage::onRecharge()
{
    const QString s = m_rechargeEdit->text().trimmed();
    bool ok = false;
    const double amount = s.toDouble(&ok);

    if (!ok || amount <= 0 || amount > 10000) {
        m_rechargeEdit->setStyleSheet(QStringLiteral("border:1px solid #EF4444;"));
        Toast::show(this, QStringLiteral("请输入合法金额(0.01~10000)"));
        return;
    }

    m_rechargeEdit->setStyleSheet(QString());
    m_rechargeEdit->clear();
    QString err;
    if (!UserService::instance().recharge(amount, &err)) {
        Toast::show(this, err);
        refresh();
        return;
    }
    Toast::show(this, QStringLiteral("充值成功"));
    refresh();
}
