#include <QtTest>

#include "core/lookup_service.h"

using namespace UnidictCore;

class LookupServiceTest : public QObject {
    Q_OBJECT
private slots:
    void not_found_message();
};

void LookupServiceTest::not_found_message() {
    LookupService svc;
    const QString out = svc.lookupDefinition("__unlikely_word__", false);
    QVERIFY(out.startsWith("Word not found"));
}

QTEST_MAIN(LookupServiceTest)
#include "lookup_service_test.moc"

