#pragma once

#include "arpspoofer.h"
#include "networkscanner.h"
#include "wequotaservice.h"

#include <QMainWindow>

class QLabel;
class QComboBox;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QSpinBox;
class QSystemTrayIcon;
class QTimer;
class QCheckBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *createDashboardPage();
    QWidget *createDevicesPage();
    QWidget *createSettingsPage();
    QWidget *createAboutPage();
    QWidget *createMetricCard(const QString &title, QLabel **valueLabel,
                              const QString &accent, int iconChar);
    void buildUi();
    void applyStyle();
    void selectPage(int index);
    void loadSettings();
    bool saveSettings();
    void refreshQuota();
    void showMessage(const QString &text, bool error);
    void showToast(const QString &text, bool error = false);
    void filterDevices();
    void populateDevices(const QList<NetworkDevice> &devices);
    void updateAdapterInfo(int index);
    void displayQuota(const QuotaInfo &info);
    void saveQuotaCache(const QuotaInfo &info);
    void loadQuotaCache();
    void setupTrayIcon();
    void applyRefreshInterval();
    static QString formatDate(const QDateTime &date);

    // ARP Spoofer methods
    void initializeSpoofer();
    void toggleDeviceSpoof(int row, bool enable);
    void updateDeviceSpeed(const QString &ip, qint64 downKBps, qint64 upKBps);
    void onDownloadCapChanged(const QString &ip, int value);
    void onUploadCapChanged(const QString &ip, int value);
    void onBlockToggled(const QString &ip, bool blocked);

    WeQuotaService m_quotaService;
    NetworkScanner m_scanner;
    ArpSpoofer m_spoofer;

    QListWidget *m_navigation = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_connectionState = nullptr;
    QLabel *m_toast = nullptr;
    QLabel *m_customerValue = nullptr;
    QLabel *m_offerValue = nullptr;
    QLabel *m_remainingValue = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_usageText = nullptr;
    QLabel *m_renewalValue = nullptr;
    QLabel *m_expiryValue = nullptr;
    QProgressBar *m_usageBar = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_spoofAllButton = nullptr;
    QTableWidget *m_devicesTable = nullptr;
    QLineEdit *m_deviceSearch = nullptr;
    QComboBox *m_deviceFilter = nullptr;
    QComboBox *m_adapterCombo = nullptr;
    QLabel *m_adapterInfo = nullptr;
    QLabel *m_deviceCount = nullptr;
    QLineEdit *m_landlineInput = nullptr;
    QLineEdit *m_passwordInput = nullptr;
    QComboBox *m_refreshIntervalCombo = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;

    QList<NetworkDevice> m_devices;
    QList<NetworkAdapter> m_adapters;

    bool m_spooferInitialized = false;
    bool m_forceQuit = false;
    bool m_trayHintShown = false;
};
