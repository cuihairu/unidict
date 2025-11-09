#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QRegularExpression>

#include "unidict_core.h"
#include "data_store.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Unidict");
    window.resize(500, 200);

    // Load dictionaries from env var if provided
    {
        const QString env = qEnvironmentVariable("UNIDICT_DICTS");
        if (!env.isEmpty()) {
            const QStringList paths = env.split(QRegularExpression("[:;]"), Qt::SkipEmptyParts);
            for (const QString& path : paths) {
                UnidictCore::DictionaryManager::instance().addDictionary(path.trimmed());
            }
        }
    }

    // Create widgets
    QLineEdit *inputField = new QLineEdit();
    inputField->setPlaceholderText("Enter a word (e.g., hello, qt, world)...");

    QPushButton *searchButton = new QPushButton("Search");
    QLabel *resultLabel = new QLabel("Definition will appear here.");
    resultLabel->setWordWrap(true);
    resultLabel->setAlignment(Qt::AlignCenter);

    if (UnidictCore::DictionaryManager::instance().getLoadedDictionaries().isEmpty()) {
        resultLabel->setText("No dictionaries loaded. Set UNIDICT_DICTS env var to paths (':' or ';' separated).\nThen search, e.g., 'hello'.");
    }

    // Layout
    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->addWidget(inputField);
    layout->addWidget(searchButton);
    layout->addWidget(resultLabel);
    window.setLayout(layout);

    // Connection (Signal & Slot)
    QObject::connect(searchButton, &QPushButton::clicked, [&]() {
        QString word = inputField->text();
        if (!word.isEmpty()) {
            QString definition = UnidictCore::searchWord(word);
            resultLabel->setText(definition);
            if (!definition.startsWith("Word not found")) {
                UnidictCore::DataStore::instance().addSearchHistory(word);
            }
        }
    });

    window.show();

    return app.exec();
}
