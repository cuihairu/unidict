#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

#include "unidict_core.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Unidict");
    window.resize(500, 200);

    // Create widgets
    QLineEdit *inputField = new QLineEdit();
    inputField->setPlaceholderText("Enter a word (e.g., hello, qt, world)...");

    QPushButton *searchButton = new QPushButton("Search");
    QLabel *resultLabel = new QLabel("Definition will appear here.");
    resultLabel->setWordWrap(true);
    resultLabel->setAlignment(Qt::AlignCenter);

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
        }
    });

    window.show();

    return app.exec();
}
