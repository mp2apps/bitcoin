// These gtk includes must be first, otherwise they will conflict with Qt
#include <unity.h>
#include <libappindicator/app-indicator.h>
#include <dbusmenuexporter.h>
// ... rest of normal includes follow
#include "unitydockiconhandler.h"


// TODO: integrate with notificator, much is shared

UnityDockIconHandler::UnityDockIconHandler(const QString &desktop_entry, QObject *parent) :
    QObject(parent)
{
    UnityLauncherEntry *launcher_entry = unity_launcher_entry_get_for_desktop_file(desktop_entry.toStdString().c_str());
    unity_launcher_entry_set_count(launcher_entry, 66);
    unity_launcher_entry_set_count_visible(launcher_entry, TRUE);

    // TODO: unity-ish icon
    // libappindicator

    // TODO: quicklist using libdbusmenu
}
