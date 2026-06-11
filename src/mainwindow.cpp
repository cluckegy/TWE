#include "mainwindow.h"
#include "credentialstore.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QDialog>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

namespace {

class AnimatedBackdrop final : public QWidget
{
public:
    explicit AnimatedBackdrop(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("appBackground"));
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_phase += 0.006;
            if (m_phase > 6.283)
                m_phase = 0.0;
            update();
        });
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal drift = qSin(m_phase) * 16.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(39, 174, 145, 32));
        painter.drawEllipse(QPointF(width() * 0.12 + drift, height() * 0.18), 155, 155);

        painter.setBrush(QColor(27, 78, 111, 25));
        painter.drawEllipse(QPointF(width() * 0.86 - drift, height() * 0.78), 210, 210);

        const qreal offset = qCos(m_phase * 0.7) * 10.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(39, 174, 145, 45), 1.2));
        painter.drawEllipse(QPointF(width() * 0.88, height() * 0.16 + offset), 54, 54);
        painter.drawEllipse(QPointF(width() * 0.88, height() * 0.16 + offset), 72, 72);

        painter.setPen(QPen(QColor(27, 78, 111, 36), 1.0));
        for (int i = 0; i < 4; ++i) {
            const qreal y = height() * 0.72 + (i * 15) + offset;
            painter.drawLine(QPointF(24, y), QPointF(118, y - 28));
        }

        painter.setPen(QPen(QColor(39, 174, 145, 34), 1.0));
        const qreal dotShift = qSin(m_phase * 0.8) * 8.0;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 6; ++column) {
                painter.setBrush(QColor(39, 174, 145, 48));
                painter.drawEllipse(QPointF(width() * 0.08 + column * 14 + dotShift,
                                            height() * 0.88 + row * 14),
                                    2.2, 2.2);
            }
        }
    }

private:
    QTimer m_timer;
    qreal m_phase = 0.0;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    applyStyle();
    loadSettings();
    loadQuotaCache();
    setupTrayIcon();
    applyRefreshInterval();
    if (m_quotaService.hasActiveSession()) {
        QTimer::singleShot(1500, this, [this]() {
            m_quotaService.refreshSession();
        });
    }

    // ── Quota service connections ───────────────────────────────
    connect(&m_quotaService, &WeQuotaService::quotaReady, this, [this](const QuotaInfo &info) {
        displayQuota(info);
        saveQuotaCache(info);
        showMessage(QStringLiteral("تم تحديث بيانات الباقة بنجاح."), false);
        QTimer::singleShot(4200, m_status, &QLabel::hide);
    });
    connect(&m_quotaService, &WeQuotaService::captchaRequired, this, [this]() {
        showMessage(QStringLiteral("سيرفر WE يطلب رمز التحقق (Captcha)..."), false);
    });
    connect(&m_quotaService, &WeQuotaService::captchaReady, this, [this](const QString &captchaBase64, const QString &captchaToken) {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("رمز التحقق (Captcha)"));
        dialog.setMinimumWidth(320);
        dialog.setLayoutDirection(Qt::RightToLeft);

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(14);

        auto *title = new QLabel(QStringLiteral("مطلوب رمز التحقق لتسجيل الدخول"));
        title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #1E293B;"));
        title->setAlignment(Qt::AlignCenter);

        auto *imgLabel = new QLabel;
        imgLabel->setAlignment(Qt::AlignCenter);
        QByteArray data = QByteArray::fromBase64(captchaBase64.toUtf8());
        QPixmap pixmap;
        pixmap.loadFromData(data);
        imgLabel->setPixmap(pixmap);
        imgLabel->setStyleSheet(QStringLiteral("border: 1px solid #CBD5E1; border-radius: 6px; padding: 4px; background: white;"));

        auto *input = new QLineEdit;
        input->setPlaceholderText(QStringLiteral("أدخل الرمز الموضح أعلاه"));
        input->setAlignment(Qt::AlignCenter);
        input->setFont(QFont(QStringLiteral("Segoe UI"), 12));
        input->setStyleSheet(QStringLiteral("padding: 8px; border-radius: 6px; border: 1.5px solid #CBD5E1; color: #0F172A;"));

        auto *btn = new QPushButton(QStringLiteral("تأكيد تسجيل الدخول"));
        btn->setStyleSheet(QStringLiteral("background-color: #10B981; color: white; border: none; padding: 10px; border-radius: 6px; font-weight: bold;"));

        layout->addWidget(title);
        layout->addWidget(imgLabel);
        layout->addWidget(input);
        layout->addWidget(btn);

        connect(btn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(input, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

        if (dialog.exec() == QDialog::Accepted) {
            QString code = input->text().trimmed();
            if (!code.isEmpty()) {
                m_quotaService.authenticateWithCaptcha(code, captchaToken);
            } else {
                showToast(QStringLiteral("تم إلغاء تسجيل الدخول: رمز التحقق فارغ"), true);
                m_quotaService.cancelRequest();
            }
        } else {
            showToast(QStringLiteral("تم إلغاء تسجيل الدخول"), true);
            m_quotaService.cancelRequest();
        }
    });
    connect(&m_quotaService, &WeQuotaService::failed, this,
            [this](const QString &message) {
        showMessage(message, true);
        showToast(message, true);
        QTimer::singleShot(4200, m_status, &QLabel::hide);
        m_connectionState->setText(QStringLiteral("غير متصل"));
        m_connectionState->setProperty("online", false);
        m_connectionState->style()->unpolish(m_connectionState);
        m_connectionState->style()->polish(m_connectionState);
        if (m_trayIcon && !isVisible())
            m_trayIcon->showMessage(QStringLiteral("TWE"), message,
                                    QSystemTrayIcon::Warning, 4500);
    });
    connect(&m_quotaService, &WeQuotaService::progressChanged, this,
            [this](const QString &message) {
        showMessage(message, false);
        m_connectionState->setText(QStringLiteral("جارٍ الاتصال"));
    });
    connect(&m_quotaService, &WeQuotaService::serverResponse, this,
            [this](const QString &message, bool success) {
        if (success)
            showToast(message, false);
        if (success) {
            m_connectionState->setText(QStringLiteral("متصل بخدمة WE"));
            m_connectionState->setProperty("online", true);
            m_connectionState->style()->unpolish(m_connectionState);
            m_connectionState->style()->polish(m_connectionState);
        }
    });
    connect(&m_quotaService, &WeQuotaService::busyChanged, this, [this](bool busy) {
        m_refreshButton->setEnabled(!busy);
        m_refreshButton->setText(busy ? QString(QChar(0xf021)) + QStringLiteral("  جارٍ التحديث...")
                                      : QString(QChar(0xf2f9)) + QStringLiteral("  تحديث الاستهلاك"));
    });

    // ── Network scanner connections ─────────────────────────────
    connect(&m_scanner, &NetworkScanner::devicesReady, this,
            [this](const QList<NetworkDevice> &devices) {
        m_devices = devices;
        populateDevices(devices);
    });
    connect(&m_scanner, &NetworkScanner::failed, this,
            [this](const QString &message) {
        m_deviceCount->setText(message);
    });
    connect(&m_scanner, &NetworkScanner::busyChanged, this, [this](bool busy) {
        m_scanButton->setEnabled(!busy);
        m_adapterCombo->setEnabled(!busy);
        m_scanButton->setText(busy ? QStringLiteral("جارٍ البحث...")
                                   : QStringLiteral("فحص الأجهزة"));
    });

    // ── ARP Spoofer connections ─────────────────────────────────
    connect(&m_spoofer, &ArpSpoofer::speedUpdate, this, &MainWindow::updateDeviceSpeed);
    connect(&m_spoofer, &ArpSpoofer::error, this, [this](const QString &msg) {
        showToast(msg, true);
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_forceQuit && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
        if (!m_trayHintShown) {
            m_trayIcon->showMessage(
                QStringLiteral("TWE يعمل في الخلفية"),
                QStringLiteral("سيستمر التحديث التلقائي. اضغط مرتين على الأيقونة لفتح البرنامج."),
                QSystemTrayIcon::Information, 4500);
            m_trayHintShown = true;
        }
        return;
    }

    if (m_spoofer.isRunning())
        m_spoofer.stopAll();
    event->accept();
}

// ── Build UI ────────────────────────────────────────────────────

void MainWindow::buildUi()
{
    resize(1180, 760);
    setMinimumSize(980, 650);
    setWindowTitle(QStringLiteral("TWE - متابعة الإنترنت"));
    setWindowIcon(QIcon(QStringLiteral(":/src/resources/twe-logo.png")));

    auto *root = new AnimatedBackdrop(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(28, 20, 28, 24);
    rootLayout->setSpacing(16);

    // Font Awesome + Segoe UI fallback font
    QFont iconFont("Segoe UI");
    iconFont.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Font Awesome 6 Free") });

    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("topHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 14, 20, 14);
    headerLayout->setSpacing(12);

    auto *brandLogo = new QLabel;
    brandLogo->setObjectName(QStringLiteral("brandLogo"));
    brandLogo->setPixmap(QPixmap(QStringLiteral(":/src/resources/twe-logo.png"))
                             .scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *brand = new QLabel(QStringLiteral("TWE"));
    brand->setObjectName(QStringLiteral("brand"));
    QFont brandFont = iconFont;
    brandFont.setPointSize(18);
    brandFont.setBold(true);
    brand->setFont(brandFont);
    auto *brandCaption = new QLabel(QStringLiteral("إدارة إنترنت WE"));
    brandCaption->setObjectName(QStringLiteral("brandCaption"));

    m_navigation = new QListWidget;
    m_navigation->setObjectName(QStringLiteral("navigation"));
    m_navigation->setFlow(QListView::LeftToRight);
    m_navigation->setFixedHeight(46);
    m_navigation->setMinimumWidth(590);
    m_navigation->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QFont navFont = iconFont;
    navFont.setPointSize(10);
    navFont.setWeight(QFont::Medium);
    m_navigation->setFont(navFont);

    m_navigation->addItems({
        QString(QChar(0xf201)) + QStringLiteral("  الاستهلاك"),
        QString(QChar(0xf0e8)) + QStringLiteral("  الأجهزة"),
        QString(QChar(0xf013)) + QStringLiteral("  الإعدادات"),
        QString(QChar(0xf05a)) + QStringLiteral("  About Us")
    });

    m_navigation->setCurrentRow(0);
    m_navigation->setSpacing(2);
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_connectionState = new QLabel(QStringLiteral("غير متصل"));
    m_connectionState->setObjectName(QStringLiteral("connectionState"));
    m_connectionState->setProperty("online", false);
    m_connectionState->setAlignment(Qt::AlignCenter);
    m_connectionState->setMinimumWidth(120);

    headerLayout->addWidget(brandLogo);
    headerLayout->addWidget(brand);
    headerLayout->addWidget(brandCaption);
    headerLayout->addStretch();
    headerLayout->addWidget(m_navigation);
    headerLayout->addStretch();
    headerLayout->addWidget(m_connectionState);

    auto *content = new QFrame;
    content->setObjectName(QStringLiteral("content"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 22);
    contentLayout->setSpacing(12);
    m_pageTitle = new QLabel(QStringLiteral("لوحة الاستهلاك"));
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_status->hide();
    m_toast = new QLabel(root);
    m_toast->setObjectName(QStringLiteral("toast"));
    m_toast->setWordWrap(true);
    m_toast->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toast->hide();
    m_pages = new QStackedWidget;
    m_pages->addWidget(createDashboardPage());
    m_pages->addWidget(createDevicesPage());
    m_pages->addWidget(createSettingsPage());
    m_pages->addWidget(createAboutPage());
    contentLayout->addWidget(m_pageTitle);
    contentLayout->addWidget(m_status);
    contentLayout->addWidget(m_pages, 1);

    rootLayout->addWidget(header);
    rootLayout->addWidget(content, 1);
    setCentralWidget(root);

    connect(m_navigation, &QListWidget::currentRowChanged, this, &MainWindow::selectPage);
}

// ── Dashboard page ──────────────────────────────────────────────

QWidget *MainWindow::createDashboardPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *welcome = new QFrame;
    welcome->setObjectName(QStringLiteral("hero"));
    auto *welcomeLayout = new QHBoxLayout(welcome);
    welcomeLayout->setContentsMargins(24, 20, 24, 20);
    auto *welcomeText = new QVBoxLayout;
    auto *caption = new QLabel(QStringLiteral("مرحبًا بك"));
    caption->setObjectName(QStringLiteral("heroCaption"));
    m_customerValue = new QLabel(QStringLiteral("لم يتم تسجيل الدخول"));
    m_customerValue->setObjectName(QStringLiteral("heroName"));
    m_offerValue = new QLabel(QStringLiteral("الباقة: -"));
    m_offerValue->setObjectName(QStringLiteral("heroOffer"));
    welcomeText->addWidget(caption);
    welcomeText->addWidget(m_customerValue);
    welcomeText->addWidget(m_offerValue);

    QFont iconFont("Segoe UI");
    iconFont.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Font Awesome 6 Free") });

    auto *welcomeIcon = new QLabel(QString(QChar(0xf3fd))); // Speedometer
    welcomeIcon->setObjectName(QStringLiteral("welcomeIcon"));
    QFont welcomeIconFont(QStringLiteral("Font Awesome 6 Free"), 44);
    welcomeIcon->setFont(welcomeIconFont);
    welcomeIcon->setStyleSheet(QStringLiteral("color: rgba(16, 185, 129, 0.15);"));
    welcomeIcon->setAlignment(Qt::AlignCenter);

    m_refreshButton = new QPushButton(QString(QChar(0xf2f9)) + QStringLiteral("  تحديث الاستهلاك"));
    m_refreshButton->setObjectName(QStringLiteral("primaryButton"));
    m_refreshButton->setFont(iconFont);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshQuota);

    welcomeLayout->addLayout(welcomeText, 1);
    welcomeLayout->addWidget(welcomeIcon);
    welcomeLayout->addSpacing(15);
    welcomeLayout->addWidget(m_refreshButton);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(16);
    cards->addWidget(createMetricCard(QStringLiteral("المتبقي"), &m_remainingValue,
                                      QStringLiteral("#10B981"), 0xf200)); // chart-pie
    cards->addWidget(createMetricCard(QStringLiteral("إجمالي الباقة"), &m_totalValue,
                                      QStringLiteral("#0EA5E9"), 0xf0ac)); // globe

    auto *usageCard = new QFrame;
    usageCard->setObjectName(QStringLiteral("card"));
    auto *usageLayout = new QVBoxLayout(usageCard);
    usageLayout->setContentsMargins(24, 20, 24, 20);
    auto *usageTitle = new QLabel(QStringLiteral("نسبة الاستهلاك"));
    usageTitle->setObjectName(QStringLiteral("cardTitle"));
    m_usageText = new QLabel(QStringLiteral("0% مستخدم"));
    m_usageText->setObjectName(QStringLiteral("usageText"));
    m_usageBar = new QProgressBar;
    m_usageBar->setRange(0, 100);
    m_usageBar->setValue(0);
    m_usageBar->setTextVisible(false);
    usageLayout->addWidget(usageTitle);
    usageLayout->addWidget(m_usageText);
    usageLayout->addWidget(m_usageBar);

    auto *dates = new QHBoxLayout;
    dates->setSpacing(16);
    dates->addWidget(createMetricCard(QStringLiteral("تاريخ التجديد"), &m_renewalValue,
                                      QStringLiteral("#0EA5E9"), 0xf073)); // calendar
    dates->addWidget(createMetricCard(QStringLiteral("تاريخ الانتهاء"), &m_expiryValue,
                                      QStringLiteral("#10B981"), 0xf017)); // clock

    layout->addWidget(welcome);
    layout->addLayout(cards);
    layout->addWidget(usageCard);
    layout->addLayout(dates);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::createMetricCard(const QString &title, QLabel **valueLabel,
                                      const QString &accent, int iconChar)
{
    Q_UNUSED(accent)
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(22, 18, 22, 18);
    
    auto *textLayout = new QVBoxLayout;
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    *valueLabel = new QLabel(QStringLiteral("-"));
    (*valueLabel)->setObjectName(QStringLiteral("cardValue"));
    (*valueLabel)->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(*valueLabel);
    
    auto *iconLabel = new QLabel(QString(QChar(iconChar)));
    iconLabel->setObjectName(QStringLiteral("cardIcon"));
    QFont iconFont(QStringLiteral("Font Awesome 6 Free"));
    iconFont.setStyleName(QStringLiteral("Solid"));
    iconFont.setPointSize(24);
    iconLabel->setFont(iconFont);
    iconLabel->setStyleSheet(QStringLiteral("color: rgba(16, 185, 129, 0.15);"));
    iconLabel->setAlignment(Qt::AlignCenter);
    
    layout->addLayout(textLayout, 1);
    layout->addWidget(iconLabel);
    return card;
}

// ── Devices page (with ARP Spoof controls) ──────────────────────

QWidget *MainWindow::createDevicesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // Network summary header
    auto *summary = new QFrame;
    summary->setObjectName(QStringLiteral("networkSummary"));
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(20, 16, 20, 16);
    auto *summaryText = new QVBoxLayout;
    auto *summaryTitle = new QLabel(QStringLiteral("مركز إدارة أجهزة الشبكة"));
    summaryTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *description = new QLabel(QStringLiteral(
        "اعرض الأجهزة المتصلة وتحكم في سرعتها أو اقطع الإنترنت عنها عبر ARP Spoofing."));
    description->setObjectName(QStringLiteral("mutedText"));
    m_deviceCount = new QLabel(QStringLiteral("0 جهاز"));
    m_deviceCount->setObjectName(QStringLiteral("countBadge"));
    summaryText->addWidget(summaryTitle);
    summaryText->addWidget(description);
    summaryLayout->addLayout(summaryText, 1);
    summaryLayout->addWidget(m_deviceCount);

    // Adapter card
    auto *adapterCard = new QFrame;
    adapterCard->setObjectName(QStringLiteral("adapterCard"));
    auto *adapterLayout = new QVBoxLayout(adapterCard);
    adapterLayout->setContentsMargins(18, 14, 18, 14);
    adapterLayout->setSpacing(10);
    auto *adapterTop = new QHBoxLayout;
    adapterTop->setSpacing(12);
    m_adapterCombo = new QComboBox;
    m_adapterCombo->setObjectName(QStringLiteral("wideCombo"));
    m_adapterCombo->setMinimumWidth(360);
    m_adapterCombo->setMinimumHeight(44);
    m_adapterInfo = new QLabel;
    m_adapterInfo->setObjectName(QStringLiteral("adapterInfo"));
    m_adapterInfo->setWordWrap(true);
    m_adapters = m_scanner.adapters();
    for (const NetworkAdapter &adapter : m_adapters) {
        m_adapterCombo->addItem(
            QStringLiteral("%1  -  %2").arg(adapter.name, adapter.type));
    }
    connect(m_adapterCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateAdapterInfo);
    
    QFont iconFont("Segoe UI");
    iconFont.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Font Awesome 6 Free") });

    auto *adapterLabel = new QLabel(QStringLiteral("كارت الشبكة"));
    adapterLabel->setObjectName(QStringLiteral("fieldLabel"));
    adapterTop->addWidget(adapterLabel);
    adapterTop->addWidget(m_adapterCombo, 1);
    adapterLayout->addLayout(adapterTop);
    adapterLayout->addWidget(m_adapterInfo);
    updateAdapterInfo(m_adapterCombo->currentIndex());

    auto *toolbarCard = new QFrame;
    toolbarCard->setObjectName(QStringLiteral("toolbarCard"));
    auto *toolbar = new QHBoxLayout(toolbarCard);
    toolbar->setContentsMargins(14, 12, 14, 12);
    toolbar->setSpacing(10);
    m_deviceSearch = new QLineEdit;
    m_deviceSearch->setObjectName(QStringLiteral("searchInput"));
    m_deviceSearch->setMinimumHeight(44);
    m_deviceSearch->setPlaceholderText(QStringLiteral("ابحث بعنوان IP أو MAC أو نوع الجهاز..."));
    m_deviceSearch->setClearButtonEnabled(true);
    m_deviceFilter = new QComboBox;
    m_deviceFilter->setObjectName(QStringLiteral("filterCombo"));
    m_deviceFilter->setMinimumHeight(44);
    m_deviceFilter->setMinimumWidth(170);
    m_deviceFilter->addItems({QStringLiteral("كل الأجهزة"), QStringLiteral("الأجهزة النشطة"),
                              QStringLiteral("الراوتر فقط")});
    m_scanButton = new QPushButton(QString(QChar(0xf002)) + QStringLiteral("  فحص الأجهزة"));
    m_scanButton->setObjectName(QStringLiteral("primaryButton"));
    m_scanButton->setMinimumHeight(44);
    m_scanButton->setFont(iconFont);
    connect(m_scanButton, &QPushButton::clicked, this, [this]() {
        const int index = m_adapterCombo->currentIndex();
        if (index < 0 || index >= m_adapters.size()) {
            m_deviceCount->setText(QStringLiteral("لا يوجد كارت شبكة متصل"));
            return;
        }
        m_scanner.scan(m_adapters.at(index));
    });
    connect(m_deviceSearch, &QLineEdit::textChanged, this, &MainWindow::filterDevices);
    connect(m_deviceFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::filterDevices);
    toolbar->addWidget(m_scanButton);
    toolbar->addWidget(m_deviceFilter);
    toolbar->addWidget(m_deviceSearch, 1);

    // Devices table — now with 9 columns
    m_devicesTable = new QTableWidget;
    m_devicesTable->setColumnCount(9);
    
    m_devicesTable->setFont(iconFont);
    m_devicesTable->horizontalHeader()->setFont(iconFont);

    m_devicesTable->setHorizontalHeaderLabels(
        {QStringLiteral("الجهاز"), QStringLiteral("النوع"),
         QStringLiteral("IP"), QStringLiteral("MAC"),
         QStringLiteral("الحالة"),
         QStringLiteral("التنزيل"),
         QStringLiteral("الرفع"),
         QStringLiteral("حظر"),
         QStringLiteral("التحكم")});
    m_devicesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_devicesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column : {2, 3, 5, 6, 7, 8})
        m_devicesTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
    m_devicesTable->setColumnWidth(2, 115);
    m_devicesTable->setColumnWidth(3, 140);
    m_devicesTable->setColumnWidth(5, 145);
    m_devicesTable->setColumnWidth(6, 145);
    m_devicesTable->setColumnWidth(7, 55);
    m_devicesTable->setColumnWidth(8, 120);
    m_devicesTable->verticalHeader()->setVisible(false);
    m_devicesTable->verticalHeader()->setDefaultSectionSize(48);
    m_devicesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_devicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_devicesTable->setAlternatingRowColors(true);

    layout->addWidget(summary);
    layout->addWidget(adapterCard);
    layout->addWidget(toolbarCard);
    layout->addWidget(m_devicesTable, 1);
    return page;
}

// ── Settings page (improved login) ──────────────────────────────

QWidget *MainWindow::createSettingsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *intro = new QFrame;
    intro->setObjectName(QStringLiteral("settingsIntro"));
    auto *introLayout = new QHBoxLayout(intro);
    introLayout->setContentsMargins(22, 16, 22, 16);
    auto *introText = new QVBoxLayout;
    auto *introTitle = new QLabel(QStringLiteral("إعداد حسابك والتحديث التلقائي"));
    introTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *introHint = new QLabel(QStringLiteral(
        "احفظ بيانات My WE وحدد وقت التحديث المناسب. يتم حفظ كلمة المرور بشكل آمن على جهازك."));
    introHint->setObjectName(QStringLiteral("loginHint"));
    introHint->setWordWrap(true);
    introText->addWidget(introTitle);
    introText->addWidget(introHint);
    introLayout->addLayout(introText, 1);

    auto *settingsRow = new QHBoxLayout;
    settingsRow->setSpacing(18);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("settingsCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(26, 24, 26, 24);
    cardLayout->setSpacing(14);

    QFont iconFont("Segoe UI");
    iconFont.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Font Awesome 6 Free") });

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(14);
    auto *loginIcon = new QLabel;
    loginIcon->setPixmap(QPixmap(QStringLiteral(":/src/resources/twe-logo.png"))
                             .scaled(54, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *headerText = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("حساب My WE"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    auto *hint = new QLabel(QStringLiteral(
        "سجّل الدخول مرة واحدة، وبعدها يستخدم البرنامج الجلسة الحالية للتحديث بدون طلب Captcha في كل مرة."));
    hint->setObjectName(QStringLiteral("loginHint"));
    hint->setWordWrap(true);
    headerText->addWidget(title);
    headerText->addWidget(hint);
    headerLayout->addWidget(loginIcon);
    headerLayout->addLayout(headerText, 1);

    auto *formLayout = new QVBoxLayout;
    formLayout->setSpacing(10);

    auto *phoneLabel = new QLabel(QString(QChar(0xf095)) + QStringLiteral("   رقم الأرضي"));
    phoneLabel->setObjectName(QStringLiteral("fieldLabel"));
    phoneLabel->setFont(iconFont);
    m_landlineInput = new QLineEdit;
    m_landlineInput->setPlaceholderText(QStringLiteral("مثال: 02XXXXXXXX"));
    m_landlineInput->setLayoutDirection(Qt::LeftToRight);
    m_landlineInput->setMaxLength(10);

    auto *passLabel = new QLabel(QString(QChar(0xf084)) + QStringLiteral("   كلمة المرور"));
    passLabel->setObjectName(QStringLiteral("fieldLabel"));
    passLabel->setFont(iconFont);
    m_passwordInput = new QLineEdit;
    m_passwordInput->setPlaceholderText(QStringLiteral("كلمة مرور حساب My WE"));
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setLayoutDirection(Qt::LeftToRight);

    formLayout->addWidget(phoneLabel);
    formLayout->addWidget(m_landlineInput);
    formLayout->addSpacing(2);
    formLayout->addWidget(passLabel);
    formLayout->addWidget(m_passwordInput);

    auto *saveButton = new QPushButton(QString(QChar(0xf0c7)) + QStringLiteral("  حفظ وتسجيل الدخول"));
    saveButton->setObjectName(QStringLiteral("loginButton"));
    saveButton->setFont(iconFont);
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        if (saveSettings()) {
            showToast(QStringLiteral("تم حفظ بيانات الحساب."), false);
            refreshQuota();
        } else {
            showToast(QStringLiteral("فشل حفظ الإعدادات. يرجى التحقق من المدخلات."), true);
        }
    });

    cardLayout->addLayout(headerLayout);
    cardLayout->addLayout(formLayout);
    cardLayout->addWidget(saveButton);

    auto *automationCard = new QFrame;
    automationCard->setObjectName(QStringLiteral("settingsCard"));
    auto *automationLayout = new QVBoxLayout(automationCard);
    automationLayout->setContentsMargins(26, 24, 26, 24);
    automationLayout->setSpacing(12);
    auto *automationTitle = new QLabel(QStringLiteral("التحديث التلقائي"));
    automationTitle->setObjectName(QStringLiteral("sectionTitle"));
    auto *automationHint = new QLabel(QStringLiteral(
        "يعمل أثناء وجود البرنامج في شريط المهام، ويستخدم جلسة تسجيل الدخول الحالية بدون إعادة إدخال البيانات."));
    automationHint->setObjectName(QStringLiteral("loginHint"));
    automationHint->setWordWrap(true);
    auto *intervalLabel = new QLabel(QStringLiteral("تحديث بيانات الاستهلاك كل"));
    intervalLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_refreshIntervalCombo = new QComboBox;
    m_refreshIntervalCombo->addItem(QStringLiteral("إيقاف التحديث التلقائي"), 0);
    m_refreshIntervalCombo->addItem(QStringLiteral("5 دقائق"), 5);
    m_refreshIntervalCombo->addItem(QStringLiteral("15 دقيقة"), 15);
    m_refreshIntervalCombo->addItem(QStringLiteral("30 دقيقة"), 30);
    m_refreshIntervalCombo->addItem(QStringLiteral("ساعة"), 60);
    connect(m_refreshIntervalCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
        QSettings settings;
        settings.setValue(QStringLiteral("we/refreshIntervalMinutes"),
                          m_refreshIntervalCombo->currentData().toInt());
        applyRefreshInterval();
    });
    automationLayout->addWidget(automationTitle);
    automationLayout->addWidget(automationHint);
    automationLayout->addSpacing(4);
    automationLayout->addWidget(intervalLabel);
    automationLayout->addWidget(m_refreshIntervalCombo);
    automationLayout->addStretch();

    settingsRow->addWidget(card, 3);
    settingsRow->addWidget(automationCard, 2);
    layout->addWidget(intro);
    layout->addLayout(settingsRow, 1);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::createAboutPage()
{
    auto *page = new QWidget;
    page->setLayoutDirection(Qt::LeftToRight);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *hero = new QFrame;
    hero->setObjectName(QStringLiteral("aboutHero"));
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(34, 28, 34, 28);
    heroLayout->setSpacing(26);

    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(QStringLiteral(":/src/resources/twe-logo.png"))
                        .scaled(92, 92, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);

    auto *copy = new QVBoxLayout;
    copy->setSpacing(8);
    auto *eyebrow = new QLabel(QStringLiteral("THANK YOU FOR DOWNLOADING TWE"));
    eyebrow->setObjectName(QStringLiteral("aboutEyebrow"));
    auto *title = new QLabel(QStringLiteral("Simple network control,\nbuilt for the community."));
    title->setObjectName(QStringLiteral("aboutTitle"));
    auto *description = new QLabel(QStringLiteral(
        "TWE was developed by Mohamed Wael (MoGlitch), sponsored by CodeLuck.\n"
        "Support for more networks is coming soon, along with new features and improvements."));
    description->setObjectName(QStringLiteral("aboutDescription"));
    description->setWordWrap(true);
    copy->addWidget(eyebrow);
    copy->addWidget(title);
    copy->addWidget(description);
    heroLayout->addLayout(copy, 1);
    heroLayout->addWidget(logo);

    auto *linksRow = new QHBoxLayout;
    linksRow->setSpacing(18);

    auto createLinkCard = [this](const QString &icon, const QString &titleText,
                                 const QString &body, const QString &buttonText,
                                 const QUrl &url) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("linkCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(24, 22, 24, 22);
        cardLayout->setSpacing(10);

        auto *iconLabel = new QLabel(icon);
        iconLabel->setObjectName(QStringLiteral("linkIcon"));
        auto *cardTitle = new QLabel(titleText);
        cardTitle->setObjectName(QStringLiteral("linkTitle"));
        auto *cardBody = new QLabel(body);
        cardBody->setObjectName(QStringLiteral("aboutDescription"));
        cardBody->setWordWrap(true);
        auto *button = new QPushButton(buttonText);
        button->setObjectName(QStringLiteral("aboutButton"));
        connect(button, &QPushButton::clicked, this, [url]() {
            QDesktopServices::openUrl(url);
        });

        cardLayout->addWidget(iconLabel);
        cardLayout->addWidget(cardTitle);
        cardLayout->addWidget(cardBody);
        cardLayout->addStretch();
        cardLayout->addWidget(button);
        return card;
    };

    linksRow->addWidget(createLinkCard(
        QStringLiteral("</>"), QStringLiteral("GitHub"),
        QStringLiteral("Explore the source code, releases, and project updates."),
        QStringLiteral("Open cluckegy/TWE"),
        QUrl(QStringLiteral("https://github.com/cluckegy/TWE"))));
    linksRow->addWidget(createLinkCard(
        QStringLiteral("#"), QStringLiteral("Discord"),
        QStringLiteral("Join the community, share feedback, and follow upcoming updates."),
        QStringLiteral("Join our Discord"),
        QUrl(QStringLiteral("https://discord.gg/YrtTNQwFrH"))));

    layout->addWidget(hero);
    layout->addLayout(linksRow, 1);
    return page;
}

// ── Page navigation ─────────────────────────────────────────────

void MainWindow::selectPage(int index)
{
    static const QStringList titles{QStringLiteral("لوحة الاستهلاك"),
                                    QStringLiteral("أجهزة الشبكة"),
                                    QStringLiteral("الإعدادات"),
                                    QStringLiteral("About Us")};
    if (index < 0 || index >= m_pages->count())
        return;
    m_pages->setCurrentIndex(index);
    m_pageTitle->setText(titles.at(index));
    if (index == 3)
        m_status->hide();
}

// ── Settings persistence ────────────────────────────────────────

void MainWindow::loadSettings()
{
    QSettings settings;
    m_landlineInput->setText(settings.value(QStringLiteral("we/landline")).toString());
    m_passwordInput->setText(CredentialStore::loadPassword());
    const int interval = settings.value(
        QStringLiteral("we/refreshIntervalMinutes"), 15).toInt();
    const int intervalIndex = m_refreshIntervalCombo->findData(interval);
    m_refreshIntervalCombo->setCurrentIndex(intervalIndex >= 0 ? intervalIndex : 2);

    settings.remove(QStringLiteral("we/directLogin"));
    settings.remove(QStringLiteral("we/subscriberId"));
    settings.remove(QStringLiteral("we/csrfToken"));
    settings.remove(QStringLiteral("we/cookies"));
}

bool MainWindow::saveSettings()
{
    const QString number = m_landlineInput->text().trimmed();
    bool allDigits = true;
    for (const QChar ch : number)
        allDigits = allDigits && ch.isDigit();
    if (number.size() != 10 || !number.startsWith(QLatin1Char('0')) || !allDigits) {
        m_landlineInput->setProperty("invalid", true);
        m_landlineInput->style()->unpolish(m_landlineInput);
        m_landlineInput->style()->polish(m_landlineInput);
        m_landlineInput->setFocus();
        return false;
    }
    m_landlineInput->setProperty("invalid", false);

    if (m_passwordInput->text().isEmpty()) {
        m_passwordInput->setProperty("invalid", true);
        m_passwordInput->style()->unpolish(m_passwordInput);
        m_passwordInput->style()->polish(m_passwordInput);
        m_passwordInput->setFocus();
        return false;
    }
    m_passwordInput->setProperty("invalid", false);

    if (!CredentialStore::savePassword(m_passwordInput->text()))
        return false;

    QSettings settings;
    settings.setValue(QStringLiteral("we/landline"), number);
    settings.setValue(QStringLiteral("we/refreshIntervalMinutes"),
                      m_refreshIntervalCombo->currentData().toInt());
    settings.sync();
    applyRefreshInterval();
    return true;
}

// ── Quota display ───────────────────────────────────────────────

void MainWindow::displayQuota(const QuotaInfo &info)
{
    m_customerValue->setText(info.customerName.isEmpty() ? QStringLiteral("-")
                                                          : info.customerName);
    m_offerValue->setText(info.offerName.isEmpty() ? QStringLiteral("-")
                                                   : info.offerName);
    m_remainingValue->setText(QStringLiteral("%1 جيجابايت").arg(info.remaining, 0, 'f', 1));
    m_totalValue->setText(QStringLiteral("%1 جيجابايت").arg(info.total, 0, 'f', 1));
    m_usageText->setText(QStringLiteral("%1% مستخدم").arg(info.usagePercentage, 0, 'f', 1));
    m_usageBar->setValue(qBound(0, qRound(info.usagePercentage), 100));
    m_renewalValue->setText(formatDate(info.renewalDate));
    const qint64 days = QDateTime::currentDateTime().daysTo(info.expiryDate);
    m_expiryValue->setText(QStringLiteral("%1  (متبقي %2 يوم)")
                               .arg(formatDate(info.expiryDate))
                               .arg(qMax<qint64>(0, days)));
}

void MainWindow::saveQuotaCache(const QuotaInfo &info)
{
    QSettings settings;
    settings.setValue(QStringLiteral("quota/name"), info.customerName);
    settings.setValue(QStringLiteral("quota/offer"), info.offerName);
    settings.setValue(QStringLiteral("quota/remaining"), info.remaining);
    settings.setValue(QStringLiteral("quota/total"), info.total);
    settings.setValue(QStringLiteral("quota/usage"), info.usagePercentage);
    settings.setValue(QStringLiteral("quota/renewal"), info.renewalDate);
    settings.setValue(QStringLiteral("quota/expiry"), info.expiryDate);
    settings.setValue(QStringLiteral("quota/updated"), QDateTime::currentDateTime());
}

void MainWindow::loadQuotaCache()
{
    QSettings settings;
    if (!settings.contains(QStringLiteral("quota/updated")))
        return;

    QuotaInfo info;
    info.customerName = settings.value(QStringLiteral("quota/name")).toString();
    info.offerName = settings.value(QStringLiteral("quota/offer")).toString();
    info.remaining = settings.value(QStringLiteral("quota/remaining")).toDouble();
    info.total = settings.value(QStringLiteral("quota/total")).toDouble();
    info.usagePercentage = settings.value(QStringLiteral("quota/usage")).toDouble();
    info.renewalDate = settings.value(QStringLiteral("quota/renewal")).toDateTime();
    info.expiryDate = settings.value(QStringLiteral("quota/expiry")).toDateTime();
    displayQuota(info);
    const QDateTime updated = settings.value(QStringLiteral("quota/updated")).toDateTime();
    m_connectionState->setText(QStringLiteral("آخر تحديث: %1")
                                   .arg(updated.toString(QStringLiteral("dd/MM hh:mm"))));
}

void MainWindow::refreshQuota()
{
    const QString number = m_landlineInput->text().trimmed();
    const QString password = m_passwordInput->text();
    if (number.isEmpty() || password.isEmpty()) {
        showMessage(QStringLiteral("اكتب رقم الأرضي وكلمة المرور في صفحة الإعدادات أولاً."), true);
        showToast(QStringLiteral("بيانات حساب My WE غير مكتملة."), true);
        m_navigation->setCurrentRow(2);
        return;
    }
    m_quotaService.checkQuota(number, password);
}

void MainWindow::applyRefreshInterval()
{
    if (!m_autoRefreshTimer) {
        m_autoRefreshTimer = new QTimer(this);
        connect(m_autoRefreshTimer, &QTimer::timeout, this, [this]() {
            if (m_quotaService.hasActiveSession())
                m_quotaService.refreshSession();
        });
    }

    const int minutes = m_refreshIntervalCombo
        ? m_refreshIntervalCombo->currentData().toInt()
        : 0;
    m_autoRefreshTimer->stop();
    if (minutes > 0)
        m_autoRefreshTimer->start(minutes * 60 * 1000);
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    const QIcon icon(QStringLiteral(":/src/resources/twe-logo.png"));
    m_trayIcon = new QSystemTrayIcon(icon, this);
    m_trayIcon->setToolTip(QStringLiteral("TWE - متابعة استهلاك الإنترنت"));

    auto *menu = new QMenu(this);
    auto *showAction = menu->addAction(QStringLiteral("فتح TWE"));
    auto *refreshAction = menu->addAction(QStringLiteral("تحديث الاستهلاك"));
    menu->addSeparator();
    auto *quitAction = menu->addAction(QStringLiteral("خروج"));
    m_trayIcon->setContextMenu(menu);

    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(refreshAction, &QAction::triggered, this, [this]() {
        if (m_quotaService.hasActiveSession())
            m_quotaService.refreshSession();
        else
            refreshQuota();
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_forceQuit = true;
        if (m_spoofer.isRunning())
            m_spoofer.stopAll();
        m_trayIcon->hide();
        qApp->quit();
    });
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick
            || reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    m_trayIcon->show();
}

// ── Messages and toasts ─────────────────────────────────────────

void MainWindow::showMessage(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setProperty("error", error);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
    m_status->show();
}

void MainWindow::showToast(const QString &text, bool error)
{
    m_toast->setText(text);
    m_toast->setProperty("error", error);
    m_toast->style()->unpolish(m_toast);
    m_toast->style()->polish(m_toast);
    m_toast->setFixedWidth(qMin(430, qMax(280, width() / 3)));
    m_toast->adjustSize();
    m_toast->move(26, height() - m_toast->height() - 34);
    m_toast->raise();
    m_toast->show();
    QTimer::singleShot(4200, m_toast, &QLabel::hide);
}

// ── Devices population (with ARP controls) ──────────────────────

void MainWindow::populateDevices(const QList<NetworkDevice> &devices)
{
    m_devicesTable->setRowCount(devices.size());
    
    QFont iconFont("Segoe UI");
    iconFont.setFamilies({ QStringLiteral("Segoe UI"), QStringLiteral("Font Awesome 6 Free") });

    for (int row = 0; row < devices.size(); ++row) {
        const NetworkDevice &device = devices.at(row);
        const bool isGateway = device.gateway;
        const bool isLocalPc = device.deviceType == QStringLiteral("هذا الكمبيوتر");

        // Icon based on type
        int typeIcon = 0xf109; // laptop
        if (isGateway || device.name.contains(QStringLiteral("Router"), Qt::CaseInsensitive)) {
            typeIcon = 0xf0e8; // sitemap / router
        } else if (device.deviceType.contains(QStringLiteral("هاتف")) || device.deviceType.contains(QStringLiteral("موبايل")) || device.deviceType.contains(QStringLiteral("Phone"), Qt::CaseInsensitive)) {
            typeIcon = 0xf3cd; // mobile
        }

        m_devicesTable->setItem(row, 0, new QTableWidgetItem(device.name));
        
        auto *typeItem = new QTableWidgetItem(QString(QChar(typeIcon)) + QStringLiteral("  ") + device.deviceType);
        typeItem->setFont(iconFont);
        m_devicesTable->setItem(row, 1, typeItem);
        
        auto *ipItem = new QTableWidgetItem(QChar(0x200E) + device.ipAddress);
        ipItem->setTextAlignment(Qt::AlignCenter);
        auto *macItem = new QTableWidgetItem(QChar(0x200E) + device.macAddress);
        macItem->setTextAlignment(Qt::AlignCenter);
        m_devicesTable->setItem(row, 2, ipItem);
        m_devicesTable->setItem(row, 3, macItem);
        m_devicesTable->setItem(row, 4, new QTableWidgetItem(device.state));

        if (isGateway || isLocalPc) {
            // Gateway and local PC: show label instead of controls
            auto *stateLabel = new QLabel(isGateway ? QString(QChar(0xf0e8)) + QStringLiteral("  بوابة الشبكة")
                                                    : QString(QChar(0xf109)) + QStringLiteral("  جهازك"));
            stateLabel->setFont(iconFont);
            stateLabel->setObjectName(QStringLiteral("qosState"));
            stateLabel->setAlignment(Qt::AlignCenter);
            m_devicesTable->setCellWidget(row, 5, stateLabel);
            m_devicesTable->setCellWidget(row, 6, new QLabel(QStringLiteral("")));
            m_devicesTable->setCellWidget(row, 7, new QLabel(QStringLiteral("")));
            m_devicesTable->setCellWidget(row, 8, new QLabel(QStringLiteral("")));
        } else {
            auto createCapControl = [this](QSpinBox **spinBox) {
                auto *container = new QFrame;
                container->setObjectName(QStringLiteral("speedControl"));
                container->setLayoutDirection(Qt::LeftToRight);
                container->setFixedHeight(34);
                auto *controlLayout = new QHBoxLayout(container);
                controlLayout->setContentsMargins(4, 3, 4, 3);
                controlLayout->setSpacing(3);

                auto *minusButton = new QToolButton;
                minusButton->setObjectName(QStringLiteral("speedStepButton"));
                minusButton->setText(QStringLiteral("-"));
                minusButton->setAutoRepeat(true);
                minusButton->setFixedSize(25, 26);

                auto *spin = new QSpinBox;
                spin->setObjectName(QStringLiteral("capSpin"));
                spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
                spin->setRange(0, 99999);
                spin->setSingleStep(64);
                spin->setSuffix(QStringLiteral(" K"));
                spin->setSpecialValueText(QStringLiteral("∞"));
                spin->setToolTip(QStringLiteral("Speed limit in KB/s. Zero means unlimited."));
                spin->setAlignment(Qt::AlignCenter);
                spin->setValue(0);
                spin->setMinimumWidth(62);

                auto *plusButton = new QToolButton;
                plusButton->setObjectName(QStringLiteral("speedStepButton"));
                plusButton->setText(QStringLiteral("+"));
                plusButton->setAutoRepeat(true);
                plusButton->setFixedSize(25, 26);

                QObject::connect(minusButton, &QToolButton::clicked, [spin]() {
                    spin->stepDown();
                });
                QObject::connect(plusButton, &QToolButton::clicked, [spin]() {
                    spin->stepUp();
                });
                controlLayout->addWidget(minusButton);
                controlLayout->addWidget(spin, 1);
                controlLayout->addWidget(plusButton);
                *spinBox = spin;
                return container;
            };

            QSpinBox *downSpin = nullptr;
            auto *downControl = createCapControl(&downSpin);
            const QString ip = device.ipAddress;
            connect(downSpin, qOverload<int>(&QSpinBox::valueChanged), this,
                    [this, ip](int val) { onDownloadCapChanged(ip, val); });

            QSpinBox *upSpin = nullptr;
            auto *upControl = createCapControl(&upSpin);
            connect(upSpin, qOverload<int>(&QSpinBox::valueChanged), this,
                    [this, ip](int val) { onUploadCapChanged(ip, val); });

            // Block checkbox
            auto *blockCheck = new QCheckBox;
            blockCheck->setToolTip(QStringLiteral("قطع الإنترنت عن هذا الجهاز"));
            auto *blockContainer = new QWidget;
            auto *blockLayout = new QHBoxLayout(blockContainer);
            blockLayout->setContentsMargins(0, 0, 0, 0);
            blockLayout->setAlignment(Qt::AlignCenter);
            blockLayout->addWidget(blockCheck);
            connect(blockCheck, &QCheckBox::toggled, this,
                    [this, ip](bool checked) { onBlockToggled(ip, checked); });

            // Spoof toggle button
            const QString mac = device.macAddress;
            auto *spoofBtn = new QPushButton;
            spoofBtn->setObjectName(QStringLiteral("spoofButton"));
            spoofBtn->setFont(iconFont);
            
            if (m_spoofer.isDeviceSpoofed(ip)) {
                spoofBtn->setText(QString(QChar(0xf04d)) + QStringLiteral("  إيقاف")); // Stop
                spoofBtn->setProperty("active", true);
            } else {
                spoofBtn->setText(QString(QChar(0xf04b)) + QStringLiteral("  تشغيل")); // Play
                spoofBtn->setProperty("active", false);
            }
            
            connect(spoofBtn, &QPushButton::clicked, this, [this, ip, mac, spoofBtn, iconFont]() {
                if (m_spoofer.isDeviceSpoofed(ip)) {
                    m_spoofer.stopSpoofing(ip);
                    spoofBtn->setText(QString(QChar(0xf04b)) + QStringLiteral("  تشغيل")); // Play
                    spoofBtn->setProperty("active", false);
                    spoofBtn->style()->unpolish(spoofBtn);
                    spoofBtn->style()->polish(spoofBtn);
                    showToast(QStringLiteral("تم إيقاف التحكم في الجهاز"), false);
                } else {
                    initializeSpoofer();
                    if (!m_spooferInitialized) return;
                    m_spoofer.startSpoofing(ip, mac);
                    spoofBtn->setText(QString(QChar(0xf04d)) + QStringLiteral("  إيقاف")); // Stop
                    spoofBtn->setProperty("active", true);
                    spoofBtn->style()->unpolish(spoofBtn);
                    spoofBtn->style()->polish(spoofBtn);
                    showToast(QStringLiteral("تم تشغيل التحكم في الجهاز"), false);
                }
            });

            m_devicesTable->setCellWidget(row, 5, downControl);
            m_devicesTable->setCellWidget(row, 6, upControl);
            m_devicesTable->setCellWidget(row, 7, blockContainer);
            m_devicesTable->setCellWidget(row, 8, spoofBtn);
        }
    }
    m_deviceCount->setText(QStringLiteral("%1 جهاز").arg(devices.size()));
}

// ── ARP Spoofer integration ─────────────────────────────────────

void MainWindow::initializeSpoofer()
{
    if (m_spooferInitialized) return;

    const int index = m_adapterCombo->currentIndex();
    if (index < 0 || index >= m_adapters.size()) {
        showToast(QStringLiteral("اختر كارت الشبكة أولاً"), true);
        return;
    }

    const NetworkAdapter &adapter = m_adapters.at(index);
    if (adapter.gateway.isEmpty()) {
        showToast(QStringLiteral("كارت الشبكة المختار ليس له بوابة (Gateway)"), true);
        return;
    }

    showToast(QStringLiteral("جارٍ تهيئة نظام التحكم في السرعة..."), false);

    // Use the adapter GUID to find it in Npcap
    const bool ok = m_spoofer.initialize(adapter.id, adapter.ipAddress,
                                          adapter.macAddress, adapter.gateway);
    if (!ok) {
        showToast(QStringLiteral("فشل في تهيئة Npcap. تأكد من تثبيت Npcap على الجهاز."), true);
        return;
    }

    m_spooferInitialized = true;
    showToast(QStringLiteral("تم تهيئة نظام التحكم بنجاح ✓"), false);
}

void MainWindow::updateDeviceSpeed(const QString &ip, qint64 downKBps, qint64 upKBps)
{
    // Find the row for this IP and update the speed display
    for (int row = 0; row < m_devicesTable->rowCount(); ++row) {
        auto *ipItem = m_devicesTable->item(row, 2);
        if (!ipItem) continue;
        // Remove LTR mark for comparison
        QString itemIp = ipItem->text();
        itemIp.remove(QChar(0x200E));
        if (itemIp == ip) {
            auto *stateItem = m_devicesTable->item(row, 4);
            if (stateItem) {
                stateItem->setText(QStringLiteral("⬇ %1  ⬆ %2 KB/s")
                                       .arg(downKBps).arg(upKBps));
            }
            break;
        }
    }
}

void MainWindow::onDownloadCapChanged(const QString &ip, int value)
{
    if (m_spoofer.isDeviceSpoofed(ip))
        m_spoofer.setDownloadCap(ip, value);
}

void MainWindow::onUploadCapChanged(const QString &ip, int value)
{
    if (m_spoofer.isDeviceSpoofed(ip))
        m_spoofer.setUploadCap(ip, value);
}

void MainWindow::onBlockToggled(const QString &ip, bool blocked)
{
    if (m_spoofer.isDeviceSpoofed(ip))
        m_spoofer.blockDevice(ip, blocked);
}

void MainWindow::toggleDeviceSpoof(int row, bool enable)
{
    Q_UNUSED(row); Q_UNUSED(enable);
}

// ── Device filtering ────────────────────────────────────────────

void MainWindow::filterDevices()
{
    const QString needle = m_deviceSearch->text().trimmed();
    const int filter = m_deviceFilter->currentIndex();
    QList<NetworkDevice> visible;
    for (const NetworkDevice &device : m_devices) {
        const bool textMatch = needle.isEmpty()
            || device.name.contains(needle, Qt::CaseInsensitive)
            || device.ipAddress.contains(needle, Qt::CaseInsensitive)
            || device.macAddress.contains(needle, Qt::CaseInsensitive)
            || device.deviceType.contains(needle, Qt::CaseInsensitive);
        const bool typeMatch = filter == 0
            || (filter == 1 && device.state == QStringLiteral("نشط"))
            || (filter == 2 && device.gateway);
        if (textMatch && typeMatch)
            visible.append(device);
    }
    populateDevices(visible);
}

void MainWindow::updateAdapterInfo(int index)
{
    if (index < 0 || index >= m_adapters.size()) {
        m_adapterInfo->setText(QStringLiteral("لا يوجد كارت Wi-Fi أو Ethernet متصل."));
        return;
    }
    const NetworkAdapter &adapter = m_adapters.at(index);
    m_adapterInfo->setText(QStringLiteral("IP: %1    |    البوابة: %2")
                               .arg(adapter.ipAddress,
                                    adapter.gateway.isEmpty()
                                        ? QStringLiteral("غير معروفة")
                                        : adapter.gateway));
    // Reset spoofer when adapter changes
    if (m_spooferInitialized) {
        m_spoofer.stopAll();
        m_spooferInitialized = false;
    }
}

QString MainWindow::formatDate(const QDateTime &date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd/MM/yyyy - hh:mm AP"))
                          : QStringLiteral("-");
}

// ── Stylesheet (polished buttons & improved appearance) ─────────

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        /* MainWindow, content background */
        QMainWindow, QWidget#content {
            background-color: #F4F8FB;
            color: #12304A;
        }

        /* Sidebar styling */
        QFrame#sidebar {
            background-color: #0B3155;
            border-left: 1px solid #0B3155;
        }

        QLabel#brand {
            color: #FFFFFF;
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1px;
        }

        QLabel#brandCaption {
            color: #A9C7DC;
            font-size: 13px;
            font-weight: 600;
        }

        QLabel#connectionState {
            color: #EF4444;
            background: #FEE2E2;
            border: 1px solid #FCA5A5;
            border-radius: 8px;
            padding: 10px;
            font-weight: 600;
        }
        QLabel#connectionState[online="true"] {
            color: #065F46;
            background: #D1FAE5;
            border: 1px solid #A7F3D0;
        }

        /* Sidebar navigation */
        QListWidget#navigation {
            border: none;
            outline: none;
            background: transparent;
            font-size: 14px;
        }
        QListWidget#navigation::item {
            color: #DCEAF4;
            padding: 14px 16px;
            border-radius: 10px;
            margin-bottom: 4px;
        }
        QListWidget#navigation::item:selected {
            color: #07324B;
            background: #DDF8F0;
            font-weight: 700;
        }
        QListWidget#navigation::item:hover:!selected {
            background: #16466D;
            color: #FFFFFF;
        }

        QLabel#pageTitle {
            font-size: 26px;
            font-weight: 800;
            color: #0F172A;
            padding-bottom: 2px;
        }

        QLabel#status {
            color: #065F46;
            background: #D1FAE5;
            border: 1px solid #A7F3D0;
            border-radius: 8px;
            padding: 10px 14px;
        }
        QLabel#status[error="true"] {
            color: #991B1B;
            background: #FEE2E2;
            border: 1px solid #FCA5A5;
        }

        /* Card layout */
        QFrame#hero {
            background: #FFFFFF;
            border: 1px solid #DDEAF2;
            border-radius: 16px;
        }
        QLabel#heroCaption {
            color: #10B981;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#heroName {
            color: #0F172A;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#heroOffer {
            color: #64748B;
            font-size: 14px;
        }

        QFrame#card, QFrame#settingsCard {
            background: #FFFFFF;
            border: 1px solid #DDEAF2;
            border-radius: 16px;
        }

        QFrame#networkSummary {
            background: #FFFFFF;
            border: 1px solid #DDEAF2;
            border-radius: 16px;
        }
        QFrame#adapterCard {
            background: #FFFFFF;
            border: 1px solid #DDEAF2;
            border-radius: 14px;
        }
        QFrame#toolbarCard {
            background: #FFFFFF;
            border: 1px solid #DDEAF2;
            border-radius: 14px;
        }
        QLabel#adapterInfo {
            color: #087F7F;
            background: #EAFBF8;
            border: 1px solid #C7F2EA;
            border-radius: 10px;
            padding: 11px 14px;
            font-weight: 600;
        }
        QLabel#mutedText {
            color: #64748B;
        }
        QLabel#countBadge {
            color: white;
            background: #10B981;
            border-radius: 16px;
            padding: 6px 14px;
            font-weight: 700;
        }

        QLabel#cardTitle {
            color: #64748B;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#cardValue {
            color: #0F172A;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#usageText {
            color: #10B981;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#sectionTitle {
            color: #0F172A;
            font-size: 20px;
            font-weight: 700;
        }

        /* Progress bar */
        QProgressBar {
            height: 10px;
            border: none;
            border-radius: 5px;
            background: #E2E8F0;
        }
        QProgressBar::chunk {
            border-radius: 5px;
            background: #10B981;
        }

        /* Buttons */
        QPushButton#primaryButton {
            color: white;
            background: #0EA5A8;
            border: none;
            border-radius: 10px;
            padding: 12px 24px;
            font-weight: 700;
            font-size: 14px;
        }
        QPushButton#primaryButton:hover {
            background: #0B8D91;
        }
        QPushButton#primaryButton:pressed {
            background: #047857;
        }
        QPushButton#primaryButton:disabled {
            background: #A7F3D0;
            color: #D1FAE5;
        }

        QPushButton#secondaryButton {
            color: #10B981;
            background: #FFFFFF;
            border: 1.5px solid #10B981;
            border-radius: 8px;
            padding: 10px 18px;
            font-weight: 600;
        }
        QPushButton#secondaryButton:hover {
            background: #ECFDF5;
        }
        QPushButton#secondaryButton:pressed {
            background: #D1FAE5;
        }

        QPushButton#loginButton {
            color: white;
            background: #0EA5A8;
            border: none;
            border-radius: 10px;
            padding: 14px 32px;
            font-weight: 700;
            font-size: 15px;
        }
        QPushButton#loginButton:hover {
            background: #0B8D91;
        }
        QPushButton#loginButton:pressed {
            background: #047857;
        }

        /* Spoof control button */
        QPushButton#spoofButton {
            color: white;
            background: #10B981;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 700;
            font-size: 12px;
        }
        QPushButton#spoofButton:hover {
            background: #059669;
        }
        QPushButton#spoofButton[active="true"] {
            background: #EF4444;
        }
        QPushButton#spoofButton[active="true"]:hover {
            background: #DC2626;
        }

        /* Inputs */
        QLineEdit, QComboBox, QSpinBox {
            background: #F9FCFE;
            border: 1.5px solid #C9DCE8;
            border-radius: 10px;
            padding: 10px 14px;
            color: #0F172A;
            font-size: 14px;
        }
        QComboBox {
            padding-left: 42px;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: left center;
            width: 34px;
            border: none;
            border-right: 1px solid #D7E5EE;
            border-top-left-radius: 9px;
            border-bottom-left-radius: 9px;
            background: #F2F8FB;
        }
        QComboBox::down-arrow {
            image: url(:/src/resources/down_arrow.png);
            width: 10px;
            height: 6px;
        }
        QComboBox::drop-down:hover {
            background: #E7F5F5;
        }
        QComboBox::down-arrow:hover {
            image: url(:/src/resources/down_arrow_hover.png);
        }
        QComboBox QAbstractItemView {
            background: #FFFFFF;
            color: #12304A;
            border: 1px solid #C9DCE8;
            border-radius: 8px;
            selection-background-color: #DDF8F0;
            selection-color: #07324B;
            padding: 6px;
            outline: none;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border: 1.5px solid #0EA5A8;
            background: #FFFFFF;
        }
        QLineEdit[invalid="true"] {
            border: 1.5px solid #EF4444;
            background: #FEFFFE;
        }

        QSpinBox#capSpin {
            min-width: 95px;
            max-width: 130px;
            padding: 6px 8px;
            font-size: 12px;
            border-radius: 6px;
            color: #0F172A;
            background-color: #FFFFFF;
        }

        /* Table Widget styling */
        QTableWidget {
            background-color: #FFFFFF;
            alternate-background-color: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
            gridline-color: #E2E8F0;
            font-size: 13px;
            color: #1E293B;
        }
        QHeaderView::section {
            background: #F1F5F9;
            color: #475569;
            border: none;
            border-bottom: 1.5px solid #E2E8F0;
            padding: 12px;
            font-weight: 700;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 10px;
        }

        /* Checkbox (Block Indicator) */
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border-radius: 5px;
            border: 2px solid #CBD5E1;
            background: #FFFFFF;
        }
        QCheckBox::indicator:checked {
            background: #EF4444;
            border-color: #EF4444;
            image: none;
        }
        QCheckBox::indicator:hover {
            border-color: #EF4444;
        }

        /* Toast styling */
        QLabel#toast {
            color: #FFFFFF;
            background: #10B981;
            border-radius: 10px;
            padding: 12px 18px;
            font-weight: 600;
        }
        QLabel#toast[error="true"] {
            background: #EF4444;
        }

        QLabel#qosState {
            color: #B45309;
            background: #FEF3C7;
            border: 1px solid #FCD34D;
            border-radius: 8px;
            padding: 8px;
            font-weight: 600;
        }

        QLabel#loginIcon {
            font-size: 44px;
            color: #10B981;
        }
        QLabel#loginHint {
            color: #64748B;
            font-size: 13px;
        }
        QLabel#fieldLabel {
            color: #1E293B;
            font-size: 14px;
            font-weight: 700;
        }
        QFrame#separator {
            background: #E2E8F0;
            border: none;
        }

    )"));

    setStyleSheet(styleSheet() + QStringLiteral(R"(
        /* Quiet, lightweight visual system */
        QMainWindow {
            background-color: #F4F7F6;
            color: #18332F;
        }
        QWidget#appBackground {
            background-color: #F4F7F6;
        }
        QFrame#topHeader {
            background: rgba(255, 255, 255, 244);
            border: 1px solid #E0E9E6;
            border-radius: 16px;
        }
        QFrame#content {
            background: rgba(250, 252, 251, 222);
            border: 1px solid #E0E9E6;
            border-radius: 18px;
        }
        QLabel#brand {
            color: #173D36;
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#brandCaption {
            color: #7A8E89;
            font-size: 12px;
            font-weight: 500;
            padding-right: 4px;
        }
        QLabel#connectionState {
            color: #A34141;
            background: #FFF1F1;
            border: 1px solid #F4D7D7;
            border-radius: 10px;
            padding: 8px 12px;
            font-weight: 600;
        }
        QLabel#connectionState[online="true"] {
            color: #176B56;
            background: #EAF8F3;
            border: 1px solid #CDEBE0;
        }
        QListWidget#navigation {
            border: none;
            outline: none;
            background: transparent;
            font-size: 13px;
        }
        QListWidget#navigation::item {
            color: #647A75;
            padding: 10px 16px;
            border-radius: 9px;
        }
        QListWidget#navigation::item:selected {
            color: #176B56;
            background: #E7F5F0;
            font-weight: 700;
        }
        QListWidget#navigation::item:hover:!selected {
            color: #294E46;
            background: #F0F5F3;
        }
        QLabel#pageTitle {
            color: #18332F;
            font-size: 23px;
            font-weight: 700;
            padding: 0 2px 6px 2px;
        }
        QFrame#hero, QFrame#card, QFrame#settingsCard,
        QFrame#networkSummary, QFrame#adapterCard, QFrame#toolbarCard {
            background: #FFFFFF;
            border: 1px solid #DDE8E4;
            border-radius: 14px;
        }
        QLabel#heroCaption, QLabel#usageText {
            color: #279D80;
        }
        QLabel#countBadge {
            color: #FFFFFF;
            background: #279D80;
        }
        QPushButton#primaryButton, QPushButton#loginButton {
            color: #FFFFFF;
            background: #277F6C;
            border: none;
        }
        QPushButton#primaryButton:hover, QPushButton#loginButton:hover {
            background: #1F6C5B;
        }
        QPushButton#primaryButton:pressed, QPushButton#loginButton:pressed {
            background: #185447;
        }
        QLineEdit, QComboBox, QSpinBox {
            color: #18332F;
            background: #FAFCFB;
            border: 1px solid #D4E1DD;
            border-radius: 9px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            background: #FFFFFF;
            border: 1px solid #279D80;
        }
        QTableWidget {
            color: #29443E;
            background: #FFFFFF;
            alternate-background-color: #F7FAF9;
            border: 1px solid #DDE8E4;
        }
        QHeaderView::section {
            color: #536B65;
            background: #F1F6F4;
            border-bottom: 1px solid #DDE8E4;
        }
        QProgressBar::chunk {
            background: #279D80;
        }
        QFrame#settingsIntro {
            background: rgba(231, 245, 240, 220);
            border: 1px solid #CFE6DE;
            border-radius: 14px;
        }
        QFrame#settingsCard {
            min-width: 330px;
        }
        QFrame#speedControl {
            background: #F7FAF9;
            border: 1px solid #D7E4E0;
            border-radius: 8px;
        }
        QSpinBox#capSpin {
            min-width: 62px;
            max-width: 72px;
            padding: 4px 0;
            margin: 0;
            color: #24443D;
            background: transparent;
            border: none;
            font-size: 11px;
            font-weight: 600;
        }
        QToolButton#speedStepButton {
            min-width: 25px;
            max-width: 25px;
            min-height: 26px;
            max-height: 26px;
            color: #277F6C;
            background: transparent;
            border: none;
            border-radius: 6px;
            font-size: 15px;
            font-weight: 700;
        }
        QToolButton#speedStepButton:hover {
            color: #FFFFFF;
            background: #279D80;
        }
        QToolButton#speedStepButton:pressed {
            background: #1F6C5B;
        }
        QFrame#aboutHero {
            background: rgba(255, 255, 255, 238);
            border: 1px solid #D8E7E2;
            border-radius: 18px;
        }
        QLabel#aboutEyebrow {
            color: #279D80;
            font-size: 12px;
            font-weight: 800;
            letter-spacing: 1px;
        }
        QLabel#aboutTitle {
            color: #173D36;
            font-size: 30px;
            font-weight: 800;
        }
        QLabel#aboutDescription {
            color: #647A75;
            font-size: 14px;
            line-height: 1.4;
        }
        QFrame#linkCard {
            background: rgba(255, 255, 255, 238);
            border: 1px solid #D8E7E2;
            border-radius: 16px;
        }
        QLabel#linkIcon {
            color: #279D80;
            font-size: 25px;
            font-weight: 800;
        }
        QLabel#linkTitle {
            color: #173D36;
            font-size: 20px;
            font-weight: 750;
        }
        QPushButton#aboutButton {
            color: #FFFFFF;
            background: #277F6C;
            border: none;
            border-radius: 9px;
            padding: 11px 18px;
            font-size: 13px;
            font-weight: 700;
        }
        QPushButton#aboutButton:hover {
            background: #1F6C5B;
        }
    )"));
}
