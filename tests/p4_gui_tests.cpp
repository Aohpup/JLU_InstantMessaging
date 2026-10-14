#include "InlineDialogTestLJY.h"
#include "gui/MainWindowLJY.h"
#include "gui/AvatarLJY.h"
#include "gui/AccountPageLJY.h"
#include "storage/SqliteStorageLJY.h"
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <stdexcept>

// 使用真实页面控件和隔离数据库验收，不向生产入口添加测试参数。
static void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void type(QWidget& window, const char* name, const QString& value)
{
    auto* edit = window.findChild<QLineEdit*>(name);
    check(edit != nullptr, "missing input");
    edit->setText(value);
}

static void click(QWidget& window, const char* name)
{
    auto* button = window.findChild<QPushButton*>(name);
    check(button && button->isEnabled() && button->isVisible(), "button unavailable");
    QTest::mouseClick(button, Qt::LeftButton);
    QApplication::processEvents();
}

// 预先安排确认框响应，避免测试等待人工操作。
static void answer(QMessageBox::StandardButton choice) { answerDialogLJY(choice); }

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    // offscreen 不自动加载 Windows 字体，截图使用本机微软雅黑。
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    try
    {
        const auto directory = std::filesystem::u8path(JLU_GUI_TEST_DIR);
        std::filesystem::create_directories(directory);
        const auto path = directory / std::filesystem::u8path("图形账号验收.db");
        const bool reload = app.arguments().contains("--reload");
        if (!reload) std::filesystem::remove(path);
        PlatformLJY platform;
        SqliteStorageLJY storage(path.u8string());
        if (reload) storage.load(platform);
        SessionManagerLJY session(platform);
        bool failSave = false;
        int saveCount = 0;
        MainWindowLJY window(session, [&] {
            if (failSave) throw std::runtime_error("验收模拟保存失败");
            storage.save(platform);
            ++saveCount;
        }, QString::fromStdWString(path.wstring()));
        window.show();
        QApplication::processEvents();
        check(!session.isLoggedIn(), "startup must not restore session");
        if (reload)
        {
            type(window, "loginNumber", "10001");
            type(window, "loginPassword", "Course123!");
            click(window, "loginButton");
            check(!session.isLoggedIn(), "old password rejected after database reload");
            type(window, "loginPassword", "Course456!");
            click(window, "loginButton");
            check(session.isLoggedIn(), "restart login");
            check(session.currentPerson().location() == "吉林长春", "restart profile");
            check(session.currentAccount().nickname() == "嘉烨QQ", "restart nickname");
            check(session.accounts().size() == 3, "restart services");
            check(session.boundQQNumber() == "10001", "restart binding");
            window.close();
            std::cout << "PASS P4 independent-process reload: profile, services, binding, fresh login\n";
            return 0;
        }
        window.findChild<QTabWidget*>("loginTabs")->setCurrentIndex(1);
        type(window, "registerName", "李嘉烨");
        type(window, "registerBirth", "11wds");
        type(window, "registerLocation", "长春");
        type(window, "registerNickname", "课设用户");
        type(window, "registerPassword", "short");
        window.grab().save(QString::fromStdWString((directory / "register-page.png").wstring()));
        click(window, "registerButton");
        check(platform.personCount() == 0, "invalid birth rejected");
        check(window.findChild<QLineEdit*>("registerName")->text() == "李嘉烨", "retain form on error");
        type(window, "registerBirth", "2005-03-12");
        click(window, "registerButton");
        check(platform.personCount() == 0, "invalid password rejected");
        type(window, "registerPassword", "Course123!");
        click(window, "registerButton");
        check(platform.personCount() == 0, "missing confirmation must not register");
        type(window, "registerPassword", "Course123!");
        type(window, "registerConfirmPassword", "Wrong123!");
        click(window, "registerButton");
        check(platform.personCount() == 0 && platform.accountCount() == 0, "mismatch must not register");
        check(window.findChild<QLineEdit*>("registerName")->text() == "李嘉烨", "mismatch retains profile");
        check(window.findChild<QLineEdit*>("registerConfirmPassword")->echoMode() == QLineEdit::Password,
            "confirmation masked");
        type(window, "registerPassword", "Course123!");
        type(window, "registerConfirmPassword", "Course123!");
        click(window, "registerButton");
        const auto registrationMessage = window.findChild<QLabel*>("saveStatus")->text();
        check(registrationMessage.contains("QQ") && registrationMessage.contains("10001") &&
            !registrationMessage.contains("Course123!"), "registration number visible in fixed status");
        check(QApplication::activeModalWidget() == nullptr, "registration result does not open modal");
        check(platform.personCount() == 1 && !session.isLoggedIn(), "registration without automatic login");
        check(window.findChild<QLineEdit*>("loginNumber")->text() == "10001", "registration displays account number");
        check(window.findChild<QLineEdit*>("loginPassword")->echoMode() == QLineEdit::Password, "password masked");
        type(window, "loginPassword", "Wrong123!");
        click(window, "loginButton");
        check(!session.isLoggedIn(), "wrong password rejected");
        check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#b42318"), "login failure is red");
        type(window, "loginPassword", "Course123!");
        click(window, "loginButton");
        check(session.isLoggedIn(), "login success");
        QLabel sample;
        applyAvatarColorLJY(&sample, session.currentPerson().personId());
        check(window.findChild<QLabel*>("accountAvatar")->styleSheet()==sample.styleSheet(), "account uses registered person color");
        const auto firstColor=sample.styleSheet();
        applyAvatarColorLJY(&sample, session.currentPerson().personId()+1);
        check(sample.styleSheet()!=firstColor, "next registered person gets next color");
        applyAvatarColorLJY(&sample, session.currentPerson().personId()+6);
        check(sample.styleSheet()==firstColor, "avatar palette cycles after six persons");
        check(!window.findChild<QPushButton*>("switchService0")->isEnabled(), "current service disabled");
        check(!window.findChild<QLineEdit*>("profileName")->isVisible(), "profile starts read only");
        click(window, "profileButton");
        check(!window.findChild<QPushButton*>("nicknameButton")->isVisible(), "profile editing hides nickname entry");
        type(window, "profileLocation", "取消的草稿");
        click(window, "profileCancel");
        check(session.currentPerson().location() == "长春", "profile cancel does not mutate");
        check(!window.findChild<QLineEdit*>("profileName")->isVisible(), "profile cancel collapses");
        check(window.findChild<QPushButton*>("nicknameButton")->isVisible(), "profile cancel restores nickname entry");
        click(window, "profileButton");
        type(window, "profileBirth", "2005-02-30");
        click(window, "profileButton");
        check(session.currentPerson().birthDate() == "2005-03-12", "profile invalid date rejected");
        auto* errorBar = window.findChild<QLabel*>("saveStatus");
        check(errorBar->isVisible() && errorBar->minimumHeight() >= 40, "failure shows visible horizontal bar");
        window.resize(900, 600);
        QApplication::processEvents();
        const QPoint fixedPosition = errorBar->mapTo(&window, QPoint(0, 0));
        for (auto* scroll : window.findChildren<QScrollArea*>())
            if (scroll->widget() == window.findChild<AccountPageLJY*>())
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QApplication::processEvents();
        auto* databaseLabel = window.findChild<QLabel*>("databasePath");
        check(errorBar->mapTo(&window, QPoint(0, 0)) == fixedPosition, "status does not move with page scroll");
        check(window.rect().contains(QRect(fixedPosition, errorBar->size())), "entire error bar fits small window");
        check(fixedPosition.y() + errorBar->height() <= databaseLabel->mapTo(&window, QPoint(0, 0)).y(),
            "status stays above database line");
        window.grab().save(QString::fromStdWString((directory / "fixed-error-minimum.png").wstring()));
        window.resize(1100, 780);
        QApplication::processEvents();
        type(window, "profileBirth", "2005-03-12");
        type(window, "profileLocation", "吉林长春");
        click(window, "profileButton");
        check(!window.findChild<QLineEdit*>("profileName")->isVisible(), "successful profile save collapses");
        check(errorBar->styleSheet().contains("background: transparent"), "successful edit clears error bar styling");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("尚未保存"), "feedback beside account cards");
        click(window, "nicknameButton");
        check(!window.findChild<QPushButton*>("profileButton")->isVisible(), "nickname editing hides profile entry");
        type(window, "profileNickname", "取消昵称");
        click(window, "nicknameCancel");
        check(session.currentAccount().nickname() == "课设用户", "nickname cancel preserves original");
        check(window.findChild<QPushButton*>("profileButton")->isVisible(), "nickname cancel restores profile entry");
        click(window, "nicknameButton");
        type(window, "profileNickname", "嘉烨QQ");
        click(window, "nicknameButton");
        check(!window.findChild<QLineEdit*>("profileNickname")->isVisible(), "nickname save collapses");
        check(window.findChild<QLabel*>("accountAvatar")->text() == "嘉", "avatar follows nickname first character");
        click(window, "passwordButton");
        type(window, "oldPassword", "discard draft");
        click(window, "passwordCancel");
        check(window.findChild<QLineEdit*>("oldPassword")->text().isEmpty(), "cancel clears password draft");
        click(window, "passwordButton");
        type(window, "oldPassword", "Course123!");
        type(window, "newPassword", "Course456!");
        type(window, "confirmPassword", "Different123!");
        click(window, "passwordButton");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("不一致"), "password confirmation inline");
        type(window, "confirmPassword", "Course456!");
        type(window, "oldPassword", "Wrong123!");
        click(window, "passwordButton");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("原密码错误"), "old password required");
        type(window, "oldPassword", "Course123!");
        type(window, "newPassword", "short");
        type(window, "confirmPassword", "short");
        click(window, "passwordButton");
        check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#b42318"), "invalid new password red");
        AccountServiceLJY verifier(platform);
        check(verifier.authenticate(ServiceTypeLJY::QQ, "10001", "Course123!") == session.currentPerson().personId(),
            "failed password edits preserve original");
        type(window, "newPassword", "Course456!");
        type(window, "confirmPassword", "Course456!");
        check(window.findChild<QLineEdit*>("newPassword")->echoMode() == QLineEdit::Password, "new password masked");
        click(window, "passwordButton");
        check(session.isLoggedIn() && !window.findChild<QWidget*>("passwordEditor")->isVisible(), "password success retains session and collapses");
        check(window.findChild<QLineEdit*>("newPassword")->text().isEmpty(), "password success clears inputs");
        check(QApplication::activeModalWidget() == nullptr, "account edits never open modal input");
        type(window, "openServiceNickname", "嘉烨微信");
        click(window, "openServiceButton");
        check(session.accounts().size() == 2, "open WeChat");
        check(session.currentAccount().serviceType() == ServiceTypeLJY::QQ, "opening does not switch");
        click(window, "bindButton");
        check(session.boundQQNumber().empty(), "binding before confirmation rejected");
        answer(QMessageBox::No);
        click(window, "switchService1");
        check(session.currentAccount().serviceType() == ServiceTypeLJY::QQ, "cancel switch");
        answer(QMessageBox::Yes);
        click(window, "switchService1");
        click(window, "bindButton");
        check(session.boundQQNumber() == "10001", "binding after confirmation");
        check(!window.findChild<QPushButton*>("bindButton")->isEnabled(), "binding immediately disables button");
        auto* table = window.findChild<QTableWidget*>("servicesTable");
        check(table->columnCount() == 6 && table->item(0, 4)->text() == "已确认", "QQ session status stays independent");
        check(table->cellWidget(0, 5) && !table->cellWidget(1, 5) && !table->cellWidget(2, 5), "binding buttons only in QQ final column");
        click(window, "unbindButton");
        check(window.findChild<QWidget*>("unbindConfirmation")->isVisible(), "unbind confirms inline");
        click(window, "cancelUnbind");
        check(session.boundQQNumber() == "10001", "cancel unbind keeps relation");
        click(window, "unbindButton");
        click(window, "confirmUnbind");
        check(session.boundQQNumber().empty() && window.findChild<QPushButton*>("bindButton")->isEnabled(), "unbind clears mapping and enables binding");
        check(table->item(0, 4)->text() == "已确认", "unbind does not change session status");
        storage.save(platform);
        PlatformLJY unboundPlatform;
        storage.load(unboundPlatform);
        SessionManagerLJY unboundSession(unboundPlatform);
        unboundSession.login(ServiceTypeLJY::QQ, "10001", "Course456!");
        check(unboundSession.boundQQNumber().empty(), "unbind survives SQLite reload");
        unboundSession.logout();
        unboundSession.login(ServiceTypeLJY::WeChat, "wx_20001", "Course456!");
        check(unboundSession.isLoggedIn(), "new password shared by WeChat");
        click(window, "bindButton");
        check(session.boundQQNumber() == "10001", "can rebind after unbind");
        type(window, "openServiceNickname", "嘉烨微博");
        click(window, "openServiceButton");
        check(session.accounts().size() == 3, "open Weibo");
        answer(QMessageBox::Yes);
        click(window, "switchService2");
        check(session.currentAccount().externalId() == "10001", "QQ and Weibo share number");
        click(window, "saveButton");
        check(saveCount == 1, "explicit save callback");
        window.grab().save(QString::fromStdWString((directory / "account-page.png").wstring()));
        window.resize(900, 600);
        QApplication::processEvents();
        window.grab().save(QString::fromStdWString((directory / "account-minimum.png").wstring()));
        click(window, "logoutButton");
        check(!session.isLoggedIn(), "logout clears identity");
        check(platform.personCount() == 1, "logout retains data");
        window.grab().save(QString::fromStdWString((directory / "login-page.png").wstring()));
        type(window, "loginPassword", "Course456!");
        click(window, "loginButton");
        check(!window.findChild<QPushButton*>("bindButton")->isEnabled(), "bound account button disabled after relogin");
        check(window.findChild<QLabel*>("bindingState")->text().contains("10001"), "binding number remains visible");
        failSave = true;
        click(window, "saveButton");
        check(window.findChild<QLabel*>("saveStatus")->text().contains("保存失败"), "save failure shown");
        check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("#b42318"), "save failure is red");
        failSave = false;
        click(window, "saveButton");
        check(window.findChild<QLabel*>("saveStatus")->styleSheet().contains("background: transparent"), "successful retry restores normal color");
        failSave = true;
        answer(QMessageBox::Cancel);
        window.close();
        check(window.isVisible(), "cancel failed close keeps window");
        failSave = false;
        window.close();
        check(!window.isVisible() && saveCount == 3, "close saves successfully");
        session.logout();
        bool anonymousPasswordRejected = false, anonymousUnbindRejected = false;
        try { session.changePassword("Course456!", "Course789!"); } catch (...) { anonymousPasswordRejected = true; }
        try { session.unbindWeChatFromQQ(); } catch (...) { anonymousUnbindRejected = true; }
        check(anonymousPasswordRejected && anonymousUnbindRejected, "account mutations require authenticated session");
        // 第二个窗口验证内存模式不显示可用保存入口。
        session.logout();
        MainWindowLJY memoryWindow(session, {}, "");
        memoryWindow.show();
        check(!memoryWindow.findChild<QPushButton*>("saveButton")->isEnabled(), "memory mode save disabled");
        memoryWindow.close();
        std::cout << "PASS P4 widgets: registration, login, profile, three services, binding, SQLite save, close failure, memory mode\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL P4 GUI: " << error.what() << '\n';
        return 1;
    }
}
