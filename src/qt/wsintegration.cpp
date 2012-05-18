#ifdef USE_UNITY
// These gtk includes must be first, otherwise they will conflict with Qt
//#include <unity.h>
//#include <libappindicator/app-indicator.h>
//#include <dbusmenuexporter.h>
#endif

#include <QWidget>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QDBusMessage>
#include <QApplication>
#include <QDebug>

#ifdef Q_WS_MAC
#include "macdockiconhandler.h"
#endif
#ifdef USE_UNITY
#include <dbusmenuexporter.h>
#endif

#include "wsintegration.h"
#include "notificator.h"
#include "guiconstants.h"

/* Default window system integration, using Qt defaults */
class WSIntegrationDefault : public WSIntegration
{
    Q_OBJECT
public:
    explicit WSIntegrationDefault(QWidget *parent = 0);
    ~WSIntegrationDefault();

    virtual void setIconMenu(QMenu *menu, QAction *quitAction);
    virtual Notificator *getNotificator()  { return notificator; }
public slots:
    void setTestnet(bool testnet);
private:
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    Notificator *notificator;
    QAction *toggleHideAction;
private slots:
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
};

/* MacOSX window system integration */
class WSIntegrationMacOSX : public WSIntegration
{
    Q_OBJECT
public:
    explicit WSIntegrationMacOSX(QWidget *parent = 0);

    virtual void setIconMenu(QMenu *menu, QAction *quitAction);
    virtual Notificator *getNotificator()  { return notificator; }
private:
    Notificator *notificator;
public slots:
    void setTestnet(bool testnet);
};

const QString UNITY_LAUNCHER_INTERFACE("com.canonical.Unity.LauncherEntry");
const QString UNITY_LAUNCHER_SIGNAL("Update");

/* Unity (Ubuntu) window system integration */
class WSIntegrationUnity : public WSIntegration
{
    Q_OBJECT
public:
    explicit WSIntegrationUnity(QWidget *parent = 0);
    ~WSIntegrationUnity();

    virtual void setIconMenu(QMenu *menu, QAction *quitAction);
    virtual Notificator *getNotificator() { return notificator; }
public slots:
    void setProgressVisible(bool visible);
    void setProgress(int numerator, int denominator);
    void setTestnet(bool testnet);
    void setAttentionFlag(bool attention);
    void setErrorFlag(bool error);
    void setNumConnections(int numConnections);
private:
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QMenu *quickMenu;
    Notificator *notificator;
    DBusMenuExporter *menuExporter;
    QAction *headerAction;

    QIcon trayIconBase;
    QIcon trayIconConnecting;
    QIcon trayIconAttention;
    QIcon trayIconError;

    QString appUri;
    QString appDesktopUri;

    // Status bits
    bool error;
    bool attention;
    int numConnections;

    void loadIcons();
    void updateStatus();
    void sendLauncherMessage(const QVariantMap &msg_args);
};

#include "wsintegration.moc"

/** Base class implementation ***************************************************/
WSIntegration::WSIntegration(QWidget *parent) :
    QObject(parent)
{
}
WSIntegration *WSIntegration::factory(QWidget *parent)
{
#ifdef Q_WS_MAC
    return new WSIntegrationMacOSX(parent);
#else
#ifdef USE_UNITY
    return new WSIntegrationUnity(parent);
    //return new WSIntegrationDefault(parent);
#else
    return new WSIntegrationDefault(parent);
#endif
#endif
}

/** Default implementation ******************************************************/
WSIntegrationDefault::WSIntegrationDefault(QWidget *parent) :
    WSIntegration(parent), trayIcon(0), trayMenu(0), notificator(0), toggleHideAction(0)
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));

    trayIcon->show();

    notificator = new Notificator(tr("bitcoin-qt"), trayIcon, parent);

    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("Show/Hide &Bitcoin"), this);
    connect(toggleHideAction, SIGNAL(triggered()), this, SIGNAL(iconTriggered()));
}

void WSIntegrationDefault::setIconMenu(QMenu *menu, QAction *quitAction)
{
    trayMenu = new QMenu();
    trayMenu->addAction(toggleHideAction);
    trayMenu->addActions(menu->actions());

    if(quitAction)
    {
        trayMenu->addSeparator();
        trayMenu->addAction(quitAction);
    }

    trayIcon->setContextMenu(trayMenu);
}

void WSIntegrationDefault::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        emit iconTriggered();
    }
}

void WSIntegrationDefault::setTestnet(bool testnet)
{
    if(testnet)
    {
        trayIcon->setToolTip(tr("Bitcoin client") + QString(" ") + tr("[testnet]"));
        trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
    }
    else
    {
        trayIcon->setToolTip(tr("Bitcoin client"));
        trayIcon->setIcon(QIcon(":/icons/toolbar"));
    }
}

WSIntegrationDefault::~WSIntegrationDefault()
{
    if(trayMenu)
        trayMenu->deleteLater();
}

/** MacOSX implementation *******************************************************/
WSIntegrationMacOSX::WSIntegrationMacOSX(QWidget *parent) :
    WSIntegration(parent)
{
#ifdef Q_WS_MAC
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, SIGNAL(dockIconClicked()), this, SIGNAL(iconTriggered()));
#endif
    notificator = new Notificator(tr("bitcoin-qt"), 0, parent);
}

void WSIntegrationMacOSX::setIconMenu(QMenu *menu, QAction *quitAction)
{
#ifdef Q_WS_MAC
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    QMenu *dockMenu = dockIconHandler->dockMenu();
    dockMenu->addActions(menu->actions());
#endif
}

void WSIntegrationMacOSX::setTestnet(bool testnet)
{
#ifdef Q_WS_MAC
    if(testnet)
    {
        MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
    }
    else
    {
        MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin"));
    }
#endif
}

/** Unity (Ubuntu) implementation ***********************************************/
WSIntegrationUnity::WSIntegrationUnity(QWidget *parent) :
    WSIntegration(parent), trayIcon(0), trayMenu(0), quickMenu(0), notificator(0),
    menuExporter(0), headerAction(0),
    error(false), attention(false), numConnections(0)
{
    loadIcons();
    /* On unity, the QSystemTrayIcon is hijacked by sni-qt
     * "A Qt plugin which turns all QSystemTrayIcon into StatusNotifierItems (appindicators)".
     * This means we can simply use the QSystemTrayIcon class, but in a unity-specific way.
     *
     * There are some differences:
     * - There is no icon "activation"
     * - Notifications are handled through DBUS
     */
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(trayIconBase);

    trayIcon->show();

    appUri = UNITY_APP_URI;
    appDesktopUri = QString("application://") + DESKTOP_FILE;

    notificator = new Notificator(tr("bitcoin-qt"), trayIcon, parent);

    headerAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Bitcoin"), this);
    connect(headerAction, SIGNAL(triggered()), this, SIGNAL(iconTriggered()));
    /*
    QVariantMap args;
    args.insert("count-visible", true);
    args.insert("count", (qint64)66);
    sendLauncherMessage(args);
    */
}

void WSIntegrationUnity::updateStatus()
{
    if(error)
    {
        trayIcon->setIcon(trayIconError);
    }
    else if(numConnections == 0)
    {
        trayIcon->setIcon(trayIconConnecting);
    }
    else if(attention)
    {
        trayIcon->setIcon(trayIconAttention);
    }
    else
    {
        trayIcon->setIcon(trayIconBase);
    }
}

void WSIntegrationUnity::setIconMenu(QMenu *menu, QAction *quitAction)
{
    trayMenu = new QMenu();
    trayMenu->addAction(headerAction);
    trayMenu->addActions(menu->actions());
    if(quitAction)
    {
        trayMenu->addSeparator();
        trayMenu->addAction(quitAction);
    }

    trayIcon->setContextMenu(trayMenu);

    // Dynamic quicklist
    quickMenu = new QMenu();
    quickMenu->addActions(menu->actions());
    if(menuExporter)
        menuExporter->deleteLater();
    menuExporter = new DBusMenuExporter(appUri, menu);
    QVariantMap args;
    args.insert("quicklist", appUri);
    sendLauncherMessage(args);
}

void WSIntegrationUnity::loadIcons()
{
    // Base icon (grey)
    trayIconBase.addFile(":/icons/unity-indicator/16/indicator-bitcoin", QSize(22,16));
    trayIconBase.addFile(":/icons/unity-indicator/22/indicator-bitcoin", QSize(22,22));
    trayIconBase.addFile(":/icons/unity-indicator/24/indicator-bitcoin", QSize(24,24));
    // Connecting: not connected (grey, half transparent)
    trayIconConnecting.addFile(":/icons/unity-indicator/16/indicator-bitcoin-connecting", QSize(22,16));
    trayIconConnecting.addFile(":/icons/unity-indicator/22/indicator-bitcoin-connecting", QSize(22,22));
    trayIconConnecting.addFile(":/icons/unity-indicator/24/indicator-bitcoin-connecting", QSize(24,24));
    // Attention: unseen transactions (blue)
    trayIconAttention.addFile(":/icons/unity-indicator/16/indicator-bitcoin-attention", QSize(22,16));
    trayIconAttention.addFile(":/icons/unity-indicator/22/indicator-bitcoin-attention", QSize(22,22));
    trayIconAttention.addFile(":/icons/unity-indicator/24/indicator-bitcoin-attention", QSize(24,24));
    // Error: errors occured (red)
    trayIconError.addFile(":/icons/unity-indicator/16/indicator-bitcoin-error", QSize(22,16));
    trayIconError.addFile(":/icons/unity-indicator/22/indicator-bitcoin-error", QSize(22,22));
    trayIconError.addFile(":/icons/unity-indicator/24/indicator-bitcoin-error", QSize(24,24));
}

void WSIntegrationUnity::setProgressVisible(bool visible)
{
    QVariantMap args;
    args.insert("progress-visible", visible);
    sendLauncherMessage(args);
}

void WSIntegrationUnity::setProgress(int numerator, int denominator)
{
    double progressValue = 0.0;
    if(denominator != 0)
        progressValue = (double)numerator/(double)denominator;
    QVariantMap args;
    args.insert("progress", progressValue);
    sendLauncherMessage(args);
}

void WSIntegrationUnity::setTestnet(bool testnet)
{
    // TODO: is it possible to change ubuntu dock item
    // maybe set desktop entry based on testnet/normalnet?
    if(testnet)
    {
        headerAction->setText(tr("&Bitcoin") + QString(" ") + tr("[testnet]"));
        headerAction->setIcon(QIcon(":/icons/toolbar_testnet"));
    }
    else
    {
        headerAction->setText(tr("&Bitcoin"));
        headerAction->setIcon(QIcon(":/icons/toolbar"));
    }
}

void WSIntegrationUnity::setAttentionFlag(bool attention)
{
    this->attention = attention;
    updateStatus();
}

void WSIntegrationUnity::setErrorFlag(bool error)
{
    this->error = error;
    updateStatus();
}

void WSIntegrationUnity::setNumConnections(int numConnections)
{
    this->numConnections = numConnections;
    updateStatus();
}

WSIntegrationUnity::~WSIntegrationUnity()
{
    if(trayMenu)
        trayMenu->deleteLater();
    if(quickMenu)
        quickMenu->deleteLater();
    if(menuExporter)
        menuExporter->deleteLater();
}

void WSIntegrationUnity::sendLauncherMessage(const QVariantMap &args)
{
    QDBusMessage msg = QDBusMessage::createSignal(appUri, UNITY_LAUNCHER_INTERFACE, UNITY_LAUNCHER_SIGNAL);
    QVariantList msg_args;
    msg_args << appDesktopUri << args;
    msg.setArguments(msg_args);
    // Ignore errors
    QDBusConnection::sessionBus().send(msg);
}
