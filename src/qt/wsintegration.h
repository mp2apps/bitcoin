#ifndef WSINTEGRATION_H
#define WSINTEGRATION_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QMenu;
class QAction;
class Notificator;
QT_END_NAMESPACE

/** Window system integration.
 * This interface is kept very generic on purpose. Implementations on specific operating systems can use
 * or ignore any piece of information as they wish.
 */
class WSIntegration : public QObject
{
    Q_OBJECT
public:
    explicit WSIntegration(QWidget *parent = 0);

    static WSIntegration *factory(QWidget *parent);

    /** Set menu to be displayed with dock/tray icon.
     * quitAction is optional and will be used to add a quit action
     * to the menu for operating systems that don't provide this themselves.
     */
    virtual void setIconMenu(QMenu *menu, QAction *quitAction=0) {}

    /**
     * Get notificator instance. The window system integration decides which notificator to use.
     */
    virtual Notificator *getNotificator() = 0;

public slots:
    /** Show progress in dock icon.
     */
    virtual void setProgressVisible(bool visible) {}
    /** Set progress shown in dock icon.
     *  If denominator==0, show as busy.
     */
    virtual void setProgress(int numerator, int denominator) {}
    virtual void setTestnet(bool testnet) {}
    virtual void setAttentionFlag(bool attention) {}
    virtual void setErrorFlag(bool error) {}
    virtual void setNumConnections(int connections) {}

signals:
    /** Dock or notification icon was triggered. */
    void iconTriggered();
public slots:

};

#endif // WSINTEGRATION_H
