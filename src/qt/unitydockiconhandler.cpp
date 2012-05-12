#include <unity.h>

#include "unitydockiconhandler.h"

UnityDockIconHandler::UnityDockIconHandler(const QString &desktop_entry, QObject *parent) :
    QObject(parent)
{
    UnityLauncherEntry *launcher_entry = unity_launcher_entry_get_for_desktop_file(desktop_entry.toStdString().c_str());
    unity_launcher_entry_set_count(launcher_entry, 66);
    unity_launcher_entry_set_count_visible(launcher_entry, TRUE);
}
