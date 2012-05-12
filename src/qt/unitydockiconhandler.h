#ifndef UNITYDOCKICONHANDLER_H
#define UNITYDOCKICONHANDLER_H

#include <QObject>

/**
 * @brief Unity (Ubuntu) launcher support.
 */
class UnityDockIconHandler : public QObject
{
    Q_OBJECT
public:
    explicit UnityDockIconHandler(const QString &desktop_entry, QObject *parent = 0);

signals:

public slots:

};

#endif // UNITYDOCKICONHANDLER_H
