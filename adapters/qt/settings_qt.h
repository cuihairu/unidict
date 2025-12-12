#ifndef UNIDICT_SETTINGS_QT_H
#define UNIDICT_SETTINGS_QT_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QCoreApplication>

namespace UnidictAdaptersQt {

// Thin wrapper over QSettings for QML access without importing labs module
class SettingsQt : public QObject {
    Q_OBJECT
public:
    explicit SettingsQt(QObject* parent = nullptr)
        : QObject(parent)
        , settings_(QSettings::IniFormat, QSettings::UserScope,
                    QCoreApplication::organizationName(),
                    QCoreApplication::applicationName()) {}

    Q_INVOKABLE bool getBool(const QString& key, bool def = false) const {
        return settings_.value(key, def).toBool();
    }
    Q_INVOKABLE void setBool(const QString& key, bool value) {
        settings_.setValue(key, value);
        settings_.sync();
    }
    Q_INVOKABLE QString getString(const QString& key, const QString& def = QString()) const {
        return settings_.value(key, def).toString();
    }
    Q_INVOKABLE void setString(const QString& key, const QString& value) {
        settings_.setValue(key, value);
        settings_.sync();
    }
    Q_INVOKABLE int getInt(const QString& key, int def = 0) const {
        return settings_.value(key, def).toInt();
    }
    Q_INVOKABLE void setInt(const QString& key, int value) {
        settings_.setValue(key, value);
        settings_.sync();
    }

private:
    mutable QSettings settings_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_SETTINGS_QT_H
