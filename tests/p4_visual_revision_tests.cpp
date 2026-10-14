#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/UiFeedbackLJY.h"
#include "services/DemoDataLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QFontInfo>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTest>
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isVisible() && button->isEnabled(), name);
    for (auto* scroll : window.findChildren<QScrollArea*>()) if (scroll->isAncestorOf(button)) scroll->ensureWidgetVisible(button);
    QTest::mouseClick(button, Qt::LeftButton); QApplication::processEvents();
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try {
        const auto directory = std::filesystem::u8path(JLU_GUI_TEST_DIR);
        std::filesystem::create_directories(directory);
        const auto path = directory / "visual.db";
        const bool reload = app.arguments().contains("--reload");
        if (!reload) {
            std::filesystem::remove(path);
            PlatformLJY original; loadDemoDataLJY(original);
            { SqliteStorageLJY old(path.u8string()); old.save(original); }
            sqlite3* db = nullptr; sqlite3_open(path.u8string().c_str(), &db);
            check(sqlite3_exec(db, "ALTER TABLE persons DROP COLUMN avatar_color;", nullptr, nullptr, nullptr) == SQLITE_OK, "prepare database without avatar field");
            sqlite3_close(db);
        }
        PlatformLJY platform; SqliteStorageLJY storage(path.u8string()); storage.load(platform);
        SessionManagerLJY session(platform);
        check(platform.accountCount() == 12, "old database migration preserves accounts");
        if (!reload) {
            check(platform.findPerson(2)->avatarColor() == 1, "old users retain rotation color");
            session.login(ServiceTypeLJY::QQ, "10001", "DemoOnly123!");
            try { session.updateProfile("wrong", "2000-01-01", "长春", 6); check(false,"custom color should fail"); }
            catch (const std::invalid_argument&) {}
            check(session.currentPerson().name() != "wrong", "invalid color does not partially update profile");
            session.logout();
        }
        MainWindowLJY window(session, [&]{ storage.save(platform); }, QString::fromStdWString(path.wstring()));
        window.show(); app.processEvents();
        auto input = [&](const char* name, const QString& value) { window.findChild<QLineEdit*>(name)->setText(value); };
        auto shot = [&](const char* name) { app.processEvents(); window.grab().save(QString::fromStdWString((directory / name).wstring())); };
        auto verifyFonts = [&] {
            for (auto* widget : window.findChildren<QWidget*>()) {
                if (!widget->isVisible()) continue;
                const QFontInfo font(widget->font());
                check(font.family() == "Microsoft YaHei", qPrintable(widget->objectName()+" font: "+font.family()));
                if (qobject_cast<QPushButton*>(widget) && widget->objectName() != "sidebarProfileButton")
                    check(!font.bold(), "ordinary buttons use regular weight");
            }
        };
        if (!reload) {
            window.resize(960, 640); app.processEvents();
            auto* headline = window.findChild<QLabel*>("welcomeHeadline");
            int small = headline->font().pixelSize(); verifyFonts(); shot("登录_小窗口.png");
            window.resize(1920, 1080); app.processEvents();
            check(headline->font().pixelSize() > small, "welcome typography grows with window");
            auto* card = window.findChild<QWidget*>("loginCard");
            check(card->width() > 490, "login form grows with window"); verifyFonts(); shot("登录_大窗口.png");
            window.resize(1100, 780); app.processEvents();
        }
        input("loginNumber", "10001"); input("loginPassword", "DemoOnly123!"); click(window, "loginButton");
        if (reload) {
            check(session.currentPerson().avatarColor() == 3, "selected avatar color persisted across process");
            check(window.findChild<QLabel*>("sidebarAvatar")->styleSheet().contains("#e7def2"), "sidebar restored saved color");
            session.updateProfile("演示A", "2000-01-01", "长春");
            check(session.currentPerson().avatarColor() == 3, "terminal-compatible profile update keeps color");
            session.enterService(ServiceTypeLJY::WeChat, true);
            check(session.currentPerson().avatarColor() == 3, "color shared between services");
            window.close(); std::cout << "PASS persisted color, cross-service and default profile API preservation\n"; return 0;
        }
        const char* navNames[] = {"messagesNav", "groupsNav", "friendsNav", "accountNav"};
        int lastY = -1;
        for (const auto* name : navNames) {
            auto* button = window.findChild<QPushButton*>(name);
            check(button && !button->icon().isNull() && button->y() > lastY, "four navigation icons in requested order");
            lastY = button->y();
            const QFontInfo info(button->font());
            check(info.family() == "Microsoft YaHei" && !info.bold(), "sidebar uses actual Microsoft YaHei regular");
        }
        click(window, "messagesNav"); check(window.findChild<QWidget*>("messagesPage")->isVisible(), "message entry opens placeholder");
        shot("侧栏_四项导航.png");
        window.findChild<QWidget*>("sidebar")->grab().save(QString::fromStdWString((directory / "侧栏_图标字体.png").wstring()));
        click(window, "friendsNav"); check(window.findChild<QWidget*>("friendsPage")->isVisible(), "friends route preserved");
        click(window, "sidebarProfileButton"); check(window.findChild<QPushButton*>("accountNav")->isChecked(), "identity card still opens account");
        auto* palette = window.findChild<QWidget*>("avatarColors");
        check(!palette->isVisible(), "palette hidden when not editing");
        click(window, "profileButton"); click(window, "avatarColor3");
        check(session.currentPerson().avatarColor() == 0, "selection is draft until save");
        check(window.findChild<QLabel*>("accountAvatar")->styleSheet().contains("#e7def2"), "preview chosen color");
        shot("账号_六色选择.png");
        click(window, "profileCancel"); check(session.currentPerson().avatarColor() == 0, "cancel leaves data unchanged");
        check(window.findChild<QLabel*>("accountAvatar")->styleSheet().contains("#d2e8d9"), "cancel restores preview");
        click(window, "profileButton"); click(window, "avatarColor3"); input("profileBirth", "bad date"); click(window, "profileButton");
        check(session.currentPerson().avatarColor() == 0 && palette->isVisible(), "invalid birth retains draft and does not save color");
        auto* status = window.findChild<QLabel*>("saveStatus");
        auto* effect = static_cast<FeedbackShakeEffectLJY*>(status->graphicsEffect());
        auto* animation = effect->findChild<QVariantAnimation*>("feedbackShake");
        check(animation->state() == QAbstractAnimation::Running, "error starts shake");
        // 同时检查真实像素位置，防止动画数值变化而画面仍在原位。
        const auto originalGeometry = status->geometry();
        auto redEdge = [&] {
            const auto image = window.grab().toImage().scaled(window.size());
            const auto origin = status->mapTo(&window, QPoint(0, 0));
            for (int x = origin.x() - 6; x < origin.x() + status->width() + 6; ++x)
                for (int y = origin.y() + 10; y < origin.y() + status->height() - 10; ++y)
                    if (image.pixelColor(x, y) == QColor("#fdeaea")) return x;
            throw std::runtime_error("warning background not rendered");
        };
        animation->pause(); animation->setCurrentTime(0); const int originalEdge = redEdge();
        animation->setCurrentTime(48);
        check(effect->offset() == -5 && redEdge() == originalEdge - 5, "warning pixels shift left");
        animation->setCurrentTime(96);
        check(effect->offset() == 5 && redEdge() == originalEdge + 5, "warning pixels shift right");
        animation->resume(); QTest::qWait(350);
        check(effect->offset() == 0 && redEdge() == originalEdge, "warning pixels return to original position");
        check(status->geometry() == originalGeometry, "shake preserves warning size and layout");
        check(status->minimumHeight() == 42 && status->maximumHeight() > 62, "warning no longer fixed to enlarged height");
        input("profileBirth", "2000-01-01"); click(window, "profileButton");
        check(session.currentPerson().avatarColor() == 3 && !palette->isVisible(), "save commits chosen color");
        check(effect->offset() == 0 && animation->state() == QAbstractAnimation::Stopped, "success stops shake");
        check(window.findChild<QLabel*>("sidebarAvatar")->styleSheet().contains("#e7def2"), "sidebar updates chosen color");
        verifyFonts(); shot("账号_保存配色.png");
        click(window, "groupsNav"); input("groupNumber", "1001"); click(window, "findGroupButton");
        auto* table = window.findChild<QTableWidget*>("groupMembers");
        for (int i = 0; i < table->rowCount(); ++i) if (table->item(i, 0)->text() == "10003") table->selectRow(i);
        window.findChild<QComboBox*>("groupMemberOperation")->setCurrentIndex(1);
        bool dialogOkay = false;
        QTimer::singleShot(0, [&]{
            auto* panel = activeDialogLJY();
            auto* yes = dialogButtonLJY(panel, QMessageBox::Yes); auto* cancel = dialogButtonLJY(panel, QMessageBox::Cancel);
            dialogOkay = yes->size() == cancel->size() && yes->height() == 40 &&
                panel->findChild<QLabel*>("dialogTitle")->text().contains("设置管理员") &&
                panel->findChild<QLabel*>("dialogMessage")->text().contains("10003");
            verifyFonts(); shot("群操作_简洁确认.png"); cancel->click();
        });
        click(window, "groupMemberAction"); check(dialogOkay, "equal buttons and clear operation context");
        window.resize(900, 600); app.processEvents();
        for (auto* scroll : window.findChildren<QScrollArea*>())
            if (scroll->isVisible()) check(scroll->horizontalScrollBar()->maximum() == 0, "small window has no horizontal overflow");
        click(window, "sidebarProfileButton"); shot("账号_小窗口.png"); window.close();
        std::cout << "PASS old schema upgrade, avatar draft/save/cancel, warning shake, responsive login, compact dialog, narrow layout\n";
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
    return 0;
}
