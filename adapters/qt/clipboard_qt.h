#ifndef UNIDICT_CLIPBOARD_QT_H
#define UNIDICT_CLIPBOARD_QT_H

#include <QObject>
#include <QString>

namespace UnidictAdaptersQt {

class ClipboardQt : public QObject {
    Q_OBJECT
public:
    explicit ClipboardQt(QObject* parent = nullptr);

    Q_INVOKABLE void setText(const QString& text) const;
    Q_INVOKABLE QString text() const;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_CLIPBOARD_QT_H

