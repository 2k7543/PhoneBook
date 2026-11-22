#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSizePolicy>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <QCheckBox>
#include <QClipboard>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QStyledItemDelegate>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include "service/PhoneBookRuntime.h"

//  This file builds the Qt UI and delegates business operations to PhoneBookRuntime.
namespace {

// Loads an icon bundled in resources.qrc by its resource name.
QIcon icon(const char* name) {
    return QIcon(QString(":/icons/%1").arg(name));
}

// Creates a shared icon button so action controls remain visually consistent.
QPushButton* iconButton(const char* iconName, const QString& text = {}) {
    auto* button = new QPushButton(text);
    button->setIcon(icon(iconName));
    button->setIconSize(QSize(18, 18));
    return button;
}

//  Paints each contact row, including alternating backgrounds and the custom favorite star.
class ContactRowDelegate : public QStyledItemDelegate {
public:
    explicit ContactRowDelegate(bool* favoriteMode, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), favoriteMode_(favoriteMode) {}

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QRect rowRect = option.rect.adjusted(4, 3, -4, -3);
        QColor background = (index.row() % 2 == 0)
            ? QColor("#ffffff")
            : QColor("#fbfcfe");
        if (option.state & QStyle::State_Selected)
            background = QColor("#eef0ff");

        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRoundedRect(rowRect, 7, 7);

        if (index.column() == 8) {
            bool isFavorite = index.data().toBool();
            QPointF center(rowRect.center());
            const double outer = 8.0;
            const double inner = outer * 0.45;
            QPolygonF star;
            for (int i = 0; i < 10; ++i) {
                double radius = (i % 2 == 0) ? outer : inner;
                double angle = -M_PI_2 + i * M_PI / 5.0;
                star << QPointF(center.x() + radius * cos(angle),
                                center.y() + radius * sin(angle));
            }
            painter->setPen(isFavorite
                                ? QPen(QColor("#d69e13"), 1)
                                : QPen(QColor("#a8afbd"), 1));
            painter->setBrush(isFavorite
                                  ? QColor("#ffc928")
                                  : QColor("#ffffff"));
            painter->drawPolygon(star);
            painter->restore();
            return;
        }
        painter->restore();

        QStyleOptionViewItem textOption(option);
        textOption.state &= ~QStyle::State_Selected;
        textOption.state &= ~QStyle::State_MouseOver;
        textOption.rect.adjust(8, 0, -8, 0);
        textOption.displayAlignment = Qt::AlignVCenter | Qt::AlignLeft;
        QStyledItemDelegate::paint(painter, textOption, index);
    }

private:
    bool* favoriteMode_;
};

// Collects registration details and creates an account through PhoneBookRuntime.
class CreateAccountDialog : public QDialog {
public:
    explicit CreateAccountDialog(PhoneBookRuntime& runtime, QWidget* parent = nullptr)
        : QDialog(parent), runtime_(runtime) {
        setWindowTitle("Tạo tài khoản");
        setModal(true);
        resize(390, 520);
        setStyleSheet(
            "* { font-family: 'Segoe UI'; font-size: 10pt; }"
            "QDialog { background: #f5f7fb; }"
            "QLabel#dialogTitle { color: #253047; font-size: 16pt; font-weight: 700; }"
            "QLabel#fieldLabel, QLabel#dialogHint { color: #98a0b3; }"
            "QLineEdit { min-height: 38px; border: 1px solid #e2e5ef; "
            "border-radius: 8px; background: white; padding: 6px 10px; color: #253047; }"
            "QPushButton#primary { min-height: 40px; border-radius: 8px; "
            "background: #5955d6; color: white; font-weight: 650; border: none; }"
            "QPushButton#secondary { min-height: 40px; border-radius: 8px; "
            "background: #f2f3ff; color: #5955d6; border: none; }");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(26, 22, 26, 22);
        layout->setSpacing(10);

        auto* title = new QLabel("Tạo tài khoản mới");
        title->setObjectName("dialogTitle");
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);

        auto* hint = new QLabel(
            "Điền thông tin bên dưới để đăng ký tài khoản.");
        hint->setObjectName("dialogHint");
        hint->setAlignment(Qt::AlignCenter);
        layout->addWidget(hint);

        name_ = addField(layout, "Họ và tên", "Nhập họ và tên");
        username_ = addField(layout, "Tên đăng nhập", "2–30 ký tự: chữ, số, . _ -");
        email_ = addField(layout, "Email", "name@example.com");
        password_ = addField(layout, "Mật khẩu", "Ít nhất 8 ký tự, gồm chữ và số");
        password_->setEchoMode(QLineEdit::Password);
        confirmation_ = addField(layout, "Nhập lại mật khẩu", "Nhập lại mật khẩu");
        confirmation_->setEchoMode(QLineEdit::Password);

        auto* buttons = new QHBoxLayout;
        auto* cancel = new QPushButton("Hủy");
        cancel->setObjectName("secondary");
        auto* submit = new QPushButton("Tạo tài khoản");
        submit->setObjectName("primary");
        buttons->addWidget(cancel);
        buttons->addWidget(submit);
        layout->addSpacing(4);
        layout->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(submit, &QPushButton::clicked,
                this, &CreateAccountDialog::createAccount);
    }

    QString registeredUsername() const { return registeredUsername_; }

private:
    QLineEdit* addField(QVBoxLayout* layout, const QString& label,
                        const QString& placeholder) {
        auto* fieldLabel = new QLabel(label);
        fieldLabel->setObjectName("fieldLabel");
        auto* field = new QLineEdit;
        field->setPlaceholderText(placeholder);
        layout->addWidget(fieldLabel);
        layout->addWidget(field);
        return field;
    }

    void createAccount() {
        if (name_->text().trimmed().isEmpty() ||
            username_->text().trimmed().isEmpty() ||
            email_->text().trimmed().isEmpty() ||
            password_->text().isEmpty() || confirmation_->text().isEmpty()) {
            QMessageBox::warning(this, "Thiếu thông tin",
                                 "Vui lòng nhập đầy đủ các thông tin.");
            return;
        }
        if (password_->text() != confirmation_->text()) {
            QMessageBox::warning(this, "Mật khẩu không khớp",
                                 "Hai mật khẩu vừa nhập không giống nhau.");
            return;
        }

        try {
            runtime_.registerUser(
                name_->text().trimmed().toStdString(),
                username_->text().trimmed().toStdString(),
                email_->text().trimmed().toStdString(),
                password_->text().toStdString());
            registeredUsername_ = username_->text().trimmed();
            accept();
        } catch (const std::exception& error) {
            QMessageBox::warning(this, "Không thể tạo tài khoản",
                                 QString::fromUtf8(error.what()));
        }
    }

    PhoneBookRuntime& runtime_;
    QLineEdit* name_ = nullptr;
    QLineEdit* username_ = nullptr;
    QLineEdit* email_ = nullptr;
    QLineEdit* password_ = nullptr;
    QLineEdit* confirmation_ = nullptr;
    QString registeredUsername_;
};

// Login dialog; authenticates through the runtime and stores the session ID/role.
class LoginDialog : public QDialog {
public:
    explicit LoginDialog(PhoneBookRuntime& runtime) : runtime_(runtime) {
        setWindowTitle("Đăng nhập - Phone Book");
        resize(430, 610);
        setStyleSheet(
            "* { font-family: 'Segoe UI'; font-size: 10pt; }"
            "QDialog { background: #f5f7fb; border: 1px solid #dfe3ec; }"
            "QFrame#loginCard { background: #ffffff; border: 1px solid #e8eaf0; "
            "border-radius: 12px; }"
            "QLabel#loginTitle { color: #253047; font-size: 16pt; font-weight: 700; }"
            "QLabel#loginSubtitle, QLabel#loginFooter, QLabel#fieldLabel { color: #98a0b3; }"
            "QLabel#fieldLabel { font-size: 9pt; font-weight: 650; }"
            "QLineEdit { min-height: 42px; border: 1px solid #e2e5ef; "
            "border-radius: 8px; background: white; padding: 8px 38px 8px 38px; "
            "color: #253047; }"
            "QLineEdit:focus { border: 1px solid #bfc3ef; }"
            "QCheckBox { color: #98a0b3; spacing: 6px; }"
            "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #dfe3ec; "
            "border-radius: 4px; background: white; }"
            "QCheckBox::indicator:checked { background: #5955d6; border: 1px solid #5955d6; }"
            "QPushButton#loginButton { min-height: 42px; border-radius: 8px; "
            "background: #5955d6; color: white; font-weight: 650; border: none; }"
            "QPushButton#loginButton:hover { background: #4e4bc5; }"
            "QPushButton#forgotButton, QPushButton#closeButton { border: none; "
            "background: transparent; color: #5955d6; }"
            "QPushButton#closeButton { color: #98a0b3; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(28, 18, 28, 18);
        root->setSpacing(0);

        auto* card = new QFrame;
        card->setObjectName("loginCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(22, 22, 22, 18);
        cardLayout->setSpacing(12);

        auto* topRow = new QHBoxLayout;
        auto* brand = new QLabel("Phone Book");
        brand->setStyleSheet("color:#98a0b3;font-size:9pt;");
        auto* closeButton = new QPushButton("×");
        closeButton->setObjectName("closeButton");
        closeButton->setFixedSize(26, 26);
        closeButton->setStyleSheet("font-size:20px;");
        closeButton->setCursor(Qt::PointingHandCursor);
        topRow->addWidget(brand);
        topRow->addStretch();
        topRow->addWidget(closeButton);
        cardLayout->addLayout(topRow);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

        auto* logo = new QLabel;
        logo->setPixmap(icon("book-user").pixmap(26, 26));
        logo->setAlignment(Qt::AlignCenter);
        logo->setFixedSize(44, 44);
        logo->setStyleSheet("background:#eef0ff;border-radius:22px;");
        cardLayout->addWidget(logo, 0, Qt::AlignHCenter);

        auto* title = new QLabel("Chào mừng trở lại!");
        title->setObjectName("loginTitle");
        title->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(title);
        auto* subtitle = new QLabel("Đăng nhập để quản lý danh bạ của bạn.");
        subtitle->setObjectName("loginSubtitle");
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet("font-size:9pt;");
        cardLayout->addWidget(subtitle);
        cardLayout->addSpacing(8);

        username_ = new QLineEdit("demo");
        password_ = new QLineEdit("12345678");
        password_->setEchoMode(QLineEdit::Password);
        username_->setPlaceholderText("Tên đăng nhập");
        password_->setPlaceholderText("Mật khẩu");
        username_->addAction(icon("dialog-company"), QLineEdit::LeadingPosition);
        password_->addAction(icon("dialog-x"), QLineEdit::LeadingPosition);

        bool passwordVisible = false;
        auto* passwordToggle = password_->addAction(icon("eye"), QLineEdit::TrailingPosition);
        passwordToggle->setToolTip("Hiện mật khẩu");
        connect(passwordToggle, &QAction::triggered, this, [this, passwordToggle, passwordVisible]() mutable {
            passwordVisible = !passwordVisible;
            password_->setEchoMode(passwordVisible ? QLineEdit::Normal : QLineEdit::Password);
            passwordToggle->setIcon(icon(passwordVisible ? "eye-off" : "eye"));
            passwordToggle->setToolTip(passwordVisible ? "Ẩn mật khẩu" : "Hiện mật khẩu");
        });

        auto* usernameLabel = new QLabel("Tên đăng nhập");
        usernameLabel->setObjectName("fieldLabel");
        cardLayout->addWidget(usernameLabel);
        cardLayout->addWidget(username_);
        auto* passwordLabel = new QLabel("Mật khẩu");
        passwordLabel->setObjectName("fieldLabel");
        cardLayout->addWidget(passwordLabel);
        cardLayout->addWidget(password_);

        auto* options = new QHBoxLayout;
        auto* remember = new QCheckBox("Ghi nhớ đăng nhập");
        auto* forgot = new QPushButton("Quên mật khẩu?");
        forgot->setObjectName("forgotButton");
        forgot->setCursor(Qt::PointingHandCursor);
        connect(forgot, &QPushButton::clicked,
                this, &LoginDialog::resetPasswordWithToken);
        options->addWidget(remember);
        options->addStretch();
        options->addWidget(forgot);
        cardLayout->addLayout(options);

        auto* loginButton = new QPushButton("Đăng nhập");
        loginButton->setObjectName("loginButton");
        loginButton->setCursor(Qt::PointingHandCursor);
        loginButton->setIcon(icon("chevron-right"));
        loginButton->setLayoutDirection(Qt::RightToLeft);
        //  Sends credentials to AuthService and opens a session after successful authentication.
        connect(loginButton, &QPushButton::clicked, this, [this] {
            try {
                RuntimeSession session = runtime_.authenticate(
                    username_->text().trimmed().toStdString(),
                    password_->text().toStdString());
                userId_ = session.userId;
                isAdmin_ = session.admin;
                accept();
            } catch (const std::exception& error) {
                QMessageBox message(this);
                message.setIcon(QMessageBox::Warning);
                message.setWindowTitle("Đăng nhập thất bại");
                message.setText(QString::fromUtf8(error.what()));
                message.setStandardButtons(QMessageBox::Ok);
                message.setStyleSheet(
                    "QMessageBox { background: #ffffff; color: #253047; }"
                    "QMessageBox QLabel { background: transparent; color: #253047; }"
                    "QMessageBox QPushButton { background: #f2f3ff; color: #253047; "
                    "border: 1px solid #dfe3ec; border-radius: 4px; padding: 5px 16px; }"
                    "QMessageBox QPushButton:hover { background: #eef0ff; }");
                message.exec();
            }
        });
        cardLayout->addWidget(loginButton);

        auto* registerHint = new QLabel("Chưa có tài khoản? <a href=\"#\" style=\"color:#5955d6;text-decoration:none;\">Tạo tài khoản</a>");
        registerHint->setAlignment(Qt::AlignCenter);
        registerHint->setTextFormat(Qt::RichText);
        registerHint->setOpenExternalLinks(false);
        registerHint->setStyleSheet("color:#98a0b3;font-size:9pt;");
        connect(registerHint, &QLabel::linkActivated, this, [this] {
            CreateAccountDialog dialog(runtime_, this);
            if (dialog.exec() == QDialog::Accepted) {
                username_->setText(dialog.registeredUsername());
                password_->clear();
                QMessageBox::information(
                    this, "Tạo tài khoản thành công",
                    "Tài khoản đã được tạo. Vui lòng đăng nhập bằng tên đăng nhập và mật khẩu vừa đăng ký.");
            }
        });
        cardLayout->addWidget(registerHint);

        root->addWidget(card, 1);
        auto* footer = new QLabel("Phone Book - Kết nối dễ dàng hơn");
        footer->setObjectName("loginFooter");
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet("font-size:8pt;");
        root->addSpacing(12);
        root->addWidget(footer);
    }

    int userId() const { return userId_; }
    bool isAdmin() const { return isAdmin_; }

private:
    // Lets a user reset with an admin-issued token; the service enforces expiry and one-time use.
    void resetPasswordWithToken() {
        QDialog dialog(this);
        dialog.setWindowTitle("Quên mật khẩu");
        dialog.setModal(true);
        dialog.resize(380, 260);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(
            "Liên hệ quản trị viên để nhận mã khôi phục, sau đó nhập mã và mật khẩu mới."));
        auto* token = new QLineEdit;
        token->setPlaceholderText("Mã đặt lại");
        auto* password = new QLineEdit;
        password->setPlaceholderText("Mật khẩu mới (ít nhất 8 ký tự, gồm chữ và số)");
        password->setEchoMode(QLineEdit::Password);
        auto* confirmation = new QLineEdit;
        confirmation->setPlaceholderText("Nhập lại mật khẩu mới");
        confirmation->setEchoMode(QLineEdit::Password);
        layout->addWidget(token);
        layout->addWidget(password);
        layout->addWidget(confirmation);
        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancel = new QPushButton("Hủy");
        auto* submit = new QPushButton("Đặt lại");
        buttons->addWidget(cancel);
        buttons->addWidget(submit);
        layout->addLayout(buttons);
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(submit, &QPushButton::clicked, &dialog, [this, &dialog, token,
                                                         password, confirmation] {
            if (password->text() != confirmation->text()) {
                QMessageBox::warning(&dialog, "Mật khẩu không khớp",
                                     "Hai mật khẩu vừa nhập không giống nhau.");
                return;
            }
            try {
                runtime_.resetPassword(token->text().trimmed().toStdString(),
                                       password->text().toStdString());
                dialog.accept();
            } catch (const std::exception& error) {
                QMessageBox::warning(&dialog, "Không thể đặt lại mật khẩu",
                                     QString::fromUtf8(error.what()));
            }
        });
        if (dialog.exec() == QDialog::Accepted)
            QMessageBox::information(this, "Đặt lại thành công",
                                     "Mật khẩu đã được cập nhật. Bạn có thể đăng nhập.");
    }

    PhoneBookRuntime& runtime_;
    QLineEdit* username_ = nullptr;
    QLineEdit* password_ = nullptr;
    int userId_ = 0;
    bool isAdmin_ = false;
};

//  Add/edit contact form; validates required fields before accepting input.
class ContactDialog : public QDialog {
public:
    ContactDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Thêm liên hệ");
        setModal(true);
        resize(520, 610);
        setStyleSheet(
            "QDialog { background: #ffffff; border-radius: 16px; }"
            "QLabel#dialogTitle { color: #253047; font-size: 16px; font-weight: 700; }"
            "QLabel#dialogSubtitle { color: #98a0b3; font-size: 11px; }"
            "QLabel#fieldLabel { color: #253047; font-size: 11px; font-weight: 650; }"
            "QLabel#requiredNote { color: #98a0b3; font-size: 10px; }"
            "QLineEdit, QTextEdit { border: 1px solid #e2e5ef; border-radius: 8px; "
            "background: #ffffff; color: #253047; padding: 8px 10px 8px 34px; "
            "selection-background-color: #eef0ff; }"
            "QLineEdit:focus, QTextEdit:focus { border: 1px solid #bfc3ef; }"
            "QPushButton#dialogSecondary { background: #f2f3ff; color: #5955d6; "
            "border: none; border-radius: 8px; padding: 10px 18px; }"
            "QPushButton#dialogPrimary { background: #5955d6; color: #ffffff; "
            "border: none; border-radius: 8px; padding: 10px 18px; font-weight: 650; }"
            "QPushButton#dialogClose { border: none; background: transparent; padding: 4px; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(28, 22, 28, 20);
        root->setSpacing(10);

        auto* header = new QHBoxLayout;
        auto* headerIcon = new QLabel;
        headerIcon->setPixmap(icon("dialog-contact").pixmap(22, 22));
        headerIcon->setAlignment(Qt::AlignCenter);
        headerIcon->setFixedSize(38, 38);
        headerIcon->setStyleSheet("background: #eef0ff; border-radius: 19px;");
        auto* headerText = new QWidget;
        auto* headerTextLayout = new QVBoxLayout(headerText);
        headerTextLayout->setContentsMargins(0, 0, 0, 0);
        headerTextLayout->setSpacing(2);
        auto* title = new QLabel("Thêm liên hệ");
        title->setObjectName("dialogTitle");
        auto* subtitle = new QLabel("Lưu thông tin để luôn giữ kết nối.");
        subtitle->setObjectName("dialogSubtitle");
        headerTextLayout->addWidget(title);
        headerTextLayout->addWidget(subtitle);
        auto* closeButton = new QPushButton;
        closeButton->setObjectName("dialogClose");
        closeButton->setIcon(icon("dialog-x"));
        closeButton->setIconSize(QSize(18, 18));
        closeButton->setFixedSize(30, 30);
        closeButton->setCursor(Qt::PointingHandCursor);
        header->addWidget(headerIcon);
        header->addWidget(headerText, 1);
        header->addWidget(closeButton, 0, Qt::AlignTop);
        root->addLayout(header);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

        auto* form = new QVBoxLayout;
        form->setContentsMargins(0, 8, 0, 0);
        form->setSpacing(8);

        auto addField = [this, form](const char* iconName, const QString& label,
                                     const QString& placeholder, QLineEdit*& field,
                                     bool required = false) {
            auto* labelWidget = new QLabel(label + (required ? " *" : ""));
            labelWidget->setObjectName("fieldLabel");
            field = new QLineEdit;
            field->setPlaceholderText(placeholder);
            field->setFixedHeight(42);
            field->addAction(new QAction(icon(iconName), {}, field),
                             QLineEdit::LeadingPosition);
            form->addWidget(labelWidget);
            form->addWidget(field);
        };

        lastname_ = new QLineEdit;
        firstname_ = new QLineEdit;
        phone_ = new QLineEdit;
        email_ = new QLineEdit;
        company_ = new QLineEdit;
        address_ = new QLineEdit;
        addField("dialog-company", "Họ", "Nhập họ", lastname_, true);
        addField("dialog-company", "Tên", "Nhập tên", firstname_, true);
        addField("dialog-phone", "Số điện thoại", "Nhập số điện thoại", phone_, true);
        addField("dialog-mail", "Email", "Nhập địa chỉ email", email_);
        addField("dialog-company", "Công ty", "Nhập tên công ty", company_);
        addField("dialog-pin", "Địa chỉ", "Nhập địa chỉ liên hệ", address_);

        auto* noteLabel = new QLabel("Ghi chú");
        noteLabel->setObjectName("fieldLabel");
        note_ = new QTextEdit;
        note_->setPlaceholderText("Thêm ghi chú về liên hệ này...");
        note_->setFixedHeight(82);
        form->addWidget(noteLabel);
        form->addWidget(note_);
        root->addLayout(form);

        auto* required = new QLabel("* Thông tin bắt buộc");
        required->setObjectName("requiredNote");
        root->addWidget(required);

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancelButton = new QPushButton("Hủy");
        cancelButton->setObjectName("dialogSecondary");
        cancelButton->setCursor(Qt::PointingHandCursor);
        auto* saveButton = new QPushButton;
        saveButton->setText("  Lưu liên hệ");
        saveButton->setObjectName("dialogPrimary");
        saveButton->setIcon(icon("dialog-check"));
        saveButton->setIconSize(QSize(16, 16));
        saveButton->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(cancelButton);
        buttons->addWidget(saveButton);
        root->addLayout(buttons);

        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(saveButton, &QPushButton::clicked, this, [this] {
            if (lastname_->text().trimmed().isEmpty() ||
                firstname_->text().trimmed().isEmpty() ||
                phone_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Thiếu thông tin",
                                     "Vui lòng nhập họ, tên và số điện thoại.");
                return;
            }
            accept();
        });
    }

    QLineEdit* lastname_ = nullptr;
    QLineEdit* firstname_ = nullptr;
    QLineEdit* phone_ = nullptr;
    QLineEdit* email_ = nullptr;
    QLineEdit* company_ = nullptr;
    QLineEdit* address_ = nullptr;
    QTextEdit* note_ = nullptr;
};

//  Main window connecting UI controls to PhoneBookRuntime.
class MainWindow : public QWidget {
public:
    MainWindow(PhoneBookRuntime& runtime, int userId, bool isAdmin)
        : runtime_(runtime), userId_(userId), isAdmin_(isAdmin) {
        //  Sets the window size and the application's shared color and typography rules.
        setWindowTitle("PhoneBook");
        resize(1180, 720);

        setStyleSheet(
            "QWidget { font-family: 'Segoe UI'; color: #253047; background: #ffffff; }"
            "#app { background: #f7f8fc; }"
            "#sidebar { background: #ffffff; border-right: 1px solid #e8eaf2; }"
            "#content { background: #f7f8fc; }"
            "#card { background: #ffffff; border: 1px solid #e8eaf2; border-radius: 12px; }"
            "QLabel#pageTitle { font-size: 22px; font-weight: 700; color: #1d2742; }"
            "QLabel#muted { color: #98a0b3; }"
            "QLineEdit { border: 1px solid #e2e5ef; border-radius: 8px; padding: 9px 10px; background: #ffffff; }"
            "QPushButton { border: none; border-radius: 8px; padding: 8px 12px; background: transparent; text-align: left; }"
            "QPushButton:hover { background: #f2f3ff; }"
            "QPushButton#primary { background: #5955d6; color: white; font-weight: 600; padding: 9px 14px; }"
            "QPushButton#navActive { background: #f0f1ff; color: #5955d6; font-weight: 650; }"
            "QPushButton#filterPill { background: #f8f9fc; color: #7d8497; "
            "border: 1px solid #edf0f5; border-radius: 8px; padding: 7px 10px; }"
            "QPushButton#filterPill:hover { background: #f0f1ff; color: #5955d6; }"
            "QPushButton#sortActive { background: #eef0ff; color: #5955d6; "
            "border: 1px solid #e1e4ff; border-radius: 8px; padding: 7px 10px; "
            "font-weight: 650; }"
            "QPushButton#sortActive:hover { background: #e7e9ff; color: #5955d6; }"
            "QPushButton#detailAction { background: #f8f9fc; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 7px 6px; text-align: center; }"
            "QTableView { border: none; background: white; alternate-background-color: #fbfcfe; "
            "selection-background-color: transparent; selection-color: #253047; gridline-color: transparent; "
            "outline: none; }"
            "QHeaderView::section { background: #f8f9fc; color: #7d8497; border: none; padding: 10px; font-size: 10px; }"
            "QLabel#avatar { background: #edf0ff; color: #5955d6; border-radius: 30px; font-size: 22px; font-weight: 700; }"
            "QLabel#smallAvatar { background: #eef0ff; color: #5955d6; border-radius: 18px; font-size: 10px; font-weight: 700; }");

        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        //  Sidebar containing navigation, contact groups, and import/export actions.
        auto* sidebar = new QFrame;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(210);
        auto* sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(18, 18, 18, 18);
        sideLayout->setSpacing(6);

        auto* logo = iconButton("book-user");
        logo->setText("  Danh bạ");
        logo->setStyleSheet("QPushButton { font-size: 17px; font-weight: 700; color: #5955d6; padding: 10px 8px; }");
        sideLayout->addWidget(logo);
        sideLayout->addSpacing(20);

        allContactsButton_ = iconButton("users", "  Tất cả liên hệ");
        allContactsButton_->setObjectName("navActive");
        sideLayout->addWidget(allContactsButton_);

        favoritesButton_ = iconButton("star", "  Yêu thích");
        sideLayout->addWidget(favoritesButton_);

        sideLayout->addSpacing(18);
        auto* groupHeader = new QWidget;
        auto* groupHeaderLayout = new QHBoxLayout(groupHeader);
        groupHeaderLayout->setContentsMargins(0, 0, 0, 0);
        auto* groupTitle = new QLabel("NHÓM LIÊN HỆ");
        groupTitle->setObjectName("muted");
        groupTitle->setStyleSheet("font-size: 9px;");
        addGroupButton_ = iconButton("plus", "");
        addGroupButton_->setFixedSize(24, 24);
        addGroupButton_->setIconSize(QSize(14, 14));
        addGroupButton_->setToolTip("Thêm nhóm liên hệ");
        addGroupButton_->setStyleSheet(
            "QPushButton { border-radius: 12px; background: #f2f3ff; "
            "text-align: center; padding: 4px; }");
        groupHeaderLayout->addWidget(groupTitle);
        groupHeaderLayout->addStretch();
        groupHeaderLayout->addWidget(addGroupButton_);
        sideLayout->addWidget(groupHeader);

        groupListWidget_ = new QWidget;
        groupListLayout_ = new QVBoxLayout(groupListWidget_);
        groupListLayout_->setContentsMargins(0, 0, 0, 0);
        groupListLayout_->setSpacing(2);
        sideLayout->addWidget(groupListWidget_);

        sideLayout->addSpacing(18);
        auto* managementTitle = new QLabel("QUẢN LÝ");
        managementTitle->setObjectName("muted");
        managementTitle->setStyleSheet("font-size: 9px;");
        sideLayout->addWidget(managementTitle);
        managementButton_ = iconButton("settings", "  Quản trị viên");
        managementButton_->setVisible(isAdmin_);
        sideLayout->addWidget(managementButton_);
        sideLayout->addStretch();
        sideImportButton_ = iconButton("download", "  Nhập CSV");
        sideExportButton_ = iconButton("upload", "  Xuất CSV");
        sideLayout->addWidget(sideImportButton_);
        sideLayout->addWidget(sideExportButton_);

        auto* account = new QLabel("  Demo tài khoản\n  admin");
        account->setObjectName("muted");
        account->setStyleSheet("padding: 12px 8px; border-top: 1px solid #e8eaf2;");
        sideLayout->addWidget(account);

        auto* content = new QWidget;
        content->setObjectName("content");
        auto* contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(28, 22, 28, 22);
        contentLayout->setSpacing(16);

        //  Page header showing the title and contact/CSV actions.
        auto* topLine = new QHBoxLayout;
        auto* breadcrumb = new QLabel("Danh bạ > Tất cả liên hệ");
        breadcrumb->setObjectName("muted");
        topLine->addWidget(breadcrumb);
        topLine->addStretch();
        contentLayout->addLayout(topLine);

        auto* titleRow = new QHBoxLayout;
        title_ = new QLabel("Tất cả liên hệ");
        title_->setObjectName("pageTitle");
        titleRow->addWidget(title_);
        countLabel_ = new QLabel("3");
        countLabel_->setStyleSheet("background:#eef0ff;color:#5955d6;border-radius:10px;padding:3px 8px;");
        titleRow->addWidget(countLabel_);
        titleRow->addStretch();
        titleRow_ = titleRow;

        importButton_ = iconButton("download", "  Nhập CSV");
        exportButton_ = iconButton("upload", "  Xuất CSV");
        addButton_ = iconButton("plus", "  Thêm liên hệ");
        addButton_->setIcon(icon("plus-white"));
        addButton_->setObjectName("primary");
        titleRow->addWidget(importButton_);
        titleRow->addWidget(exportButton_);
        titleRow->addWidget(addButton_);
        contentLayout->addLayout(titleRow);

        auto* cards = new QHBoxLayout;
        cards->setSpacing(16);

        // Contact-list card with search, group/favorite filtering, and sorting.
        auto* listCard = new QFrame;
        listCard->setObjectName("card");
        listCard_ = listCard;
        auto* listLayout = new QVBoxLayout(listCard);
        listLayout->setContentsMargins(18, 18, 18, 18);

        auto* searchRow = new QHBoxLayout;
        search_ = new QLineEdit;
        search_->setPlaceholderText("Tìm theo tên, số điện thoại hoặc email...");
        search_->setClearButtonEnabled(true);
        searchRow->addWidget(search_, 1);
        sortButton_ = iconButton("arrow-a-z", "  Tên A-Z");
        sortButton_->setObjectName("filterPill");
        sortButton_->setCheckable(true);
        searchRow->addWidget(sortButton_);
        listLayout->addLayout(searchRow);

        auto* filterRow = new QHBoxLayout;
        filterRow->setSpacing(8);
        groupFilter_ = new QComboBox;
        groupFilter_->setMinimumWidth(145);
        groupFilter_->setStyleSheet(
            "QComboBox { background: #f8f9fc; color: #7d8497; "
            "border: 1px solid #edf0f5; border-radius: 8px; "
            "padding: 7px 10px; font-size: 12px; }"
            "QComboBox:hover { background: #f0f1ff; color: #5955d6; }"
            "QComboBox::drop-down { border: none; width: 20px; }"
            "QComboBox::down-arrow { image: none; }"
            "QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 4px; outline: none; "
            "selection-background-color: #eef0ff; selection-color: #5955d6; }");
        auto* groupFilterView = new QListView(groupFilter_);
        groupFilterView->setIconSize(QSize(0, 0));
        groupFilterView->setTextElideMode(Qt::ElideRight);
        groupFilterView->setUniformItemSizes(true);
        groupFilterView->setStyleSheet(
            "QListView { background: #ffffff; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 4px; outline: none; }"
            "QListView::item { height: 28px; margin: 1px 0px; "
            "padding: 5px 8px; border-radius: 6px; color: #253047; }"
            "QListView::item:hover { background: #f2f3ff; color: #5955d6; }"
            "QListView::item:selected { background: #eef0ff; color: #5955d6; }");
        groupFilter_->setView(groupFilterView);
        groupFilter_->addItem("Tất cả liên hệ", -1);
        filterRow->addWidget(groupFilter_);
        filterRow->addStretch();
        listLayout->addLayout(filterRow);
        //  Group filter selector; its options are supplied by GroupService.
        connect(groupFilter_, &QComboBox::activated, this, [this](int index) {
            int filter = groupFilter_->itemData(index).toInt();
            if (filter == -1) {
                showAllContacts();
            } else {
                setFavoriteSelectionMode(false);
                QString groupName;
                for (const RuntimeGroup& group : runtime_.listGroups(userId_)) {
                    if (group.id == filter) {
                        groupName = QString::fromStdString(group.name);
                        break;
                    }
                }
                filterMode_ = 2;
                selectedGroupId_ = filter;
                title_->setText(groupName);
                clearNavSelection();
                for (QPushButton* button : groupListWidget_->findChildren<QPushButton*>()) {
                    if (button->property("groupId").toInt() == filter) {
                        button->setObjectName("navActive");
                        button->style()->unpolish(button);
                        button->style()->polish(button);
                    }
                }
                reload();
            }
        });
        //  Switches sorting between last name and first name when clicked.
        connect(sortButton_, &QPushButton::clicked, this, [this] {
            nameSortMode_ = (nameSortMode_ == 0) ? 1 : 0;
            sortButton_->setChecked(nameSortMode_ == 1);
            sortButton_->setObjectName(nameSortMode_ == 1 ? "sortActive" : "filterPill");
            sortButton_->style()->unpolish(sortButton_);
            sortButton_->style()->polish(sortButton_);
            reload();
        });

        //  The table displays runtime-provided data; its item model does not query the database.
        table_ = new QTableView;
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setAlternatingRowColors(true);
        table_->setShowGrid(false);
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->setSortingEnabled(false);
        table_->verticalHeader()->setVisible(false);
        table_->setColumnWidth(2, 260);
        table_->setItemDelegate(new ContactRowDelegate(&favoriteSelectionMode_, table_));
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->verticalHeader()->setDefaultSectionSize(48);

        model_ = new QStandardItemModel(this);
        model_->setColumnCount(11);
        model_->setHeaderData(2, Qt::Horizontal, "HỌ VÀ TÊN");
        model_->setHeaderData(3, Qt::Horizontal, "SỐ ĐIỆN THOẠI");
        model_->setHeaderData(4, Qt::Horizontal, "EMAIL");
        model_->setHeaderData(8, Qt::Horizontal, "YÊU THÍCH");
        model_->setHeaderData(9, Qt::Horizontal, "HỌ");
        model_->setHeaderData(10, Qt::Horizontal, "TÊN");
        table_->setModel(model_);
        table_->hideColumn(0);
        table_->hideColumn(1);
        table_->hideColumn(5);
        table_->hideColumn(6);
        table_->hideColumn(7);
        table_->hideColumn(9);
        table_->hideColumn(10);
        table_->setColumnWidth(8, 80);
        listLayout->addWidget(table_);

        footer_ = new QLabel("Hiển thị 3 liên hệ");
        footer_->setObjectName("muted");
        footer_->setStyleSheet("padding-top: 10px;");
        listLayout->addWidget(footer_);

        // Detail card for the selected contact, with edit, favorite, and group assignment actions.
        detailCard_ = new QFrame;
        detailCard_->setObjectName("card");
        detailCard_->setFixedWidth(330);
        detailLayout_ = new QVBoxLayout(detailCard_);
        detailLayout_->setContentsMargins(20, 24, 20, 24);
        detailLayout_->setSpacing(12);

        detailAvatar_ = new QLabel;
        detailAvatar_->setObjectName("avatar");
        detailAvatar_->setFixedSize(60, 60);
        detailAvatar_->setAlignment(Qt::AlignCenter);
        detailName_ = new QLabel;
        detailName_->setStyleSheet("font-size: 16px; font-weight: 700;");
        detailName_->setAlignment(Qt::AlignCenter);
        detailLayout_->addWidget(detailAvatar_, 0, Qt::AlignHCenter);
        detailLayout_->addWidget(detailName_);

        detailActions_ = new QHBoxLayout;
        detailActions_->setContentsMargins(0, 0, 0, 0);
        detailActions_->setSpacing(6);
        editDetailButton_ = iconButton("pencil", "  Chỉnh sửa");
        editDetailButton_->setObjectName("detailAction");
        editDetailButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        favoriteDetailButton_ = iconButton("star", "  Yêu thích");
        favoriteDetailButton_->setObjectName("detailAction");
        favoriteDetailButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        deleteDetailButton_ = iconButton("trash", "");
        deleteDetailButton_->setObjectName("detailAction");
        deleteDetailButton_->setFixedWidth(42);
        deleteDetailButton_->setIconSize(QSize(18, 18));
        deleteDetailButton_->setToolTip("Xóa liên hệ");
        detailActions_->addWidget(editDetailButton_);
        detailActions_->addWidget(favoriteDetailButton_);
        detailActions_->addWidget(deleteDetailButton_);
        detailLayout_->addLayout(detailActions_);

        detailLayout_->addSpacing(6);
        detailLayout_->addWidget(detailLine("phone", "Điện thoại", ""));
        detailLayout_->addWidget(detailLine("mail", "Email", ""));
        detailLayout_->addWidget(detailLine("pin", "Địa chỉ", ""));
        detailLayout_->addWidget(detailLine("cake", "Ngày sinh", "Chưa cập nhật"));
        detailLayout_->addStretch();

        auto* groupRow = new QWidget;
        auto* groupRowLayout = new QHBoxLayout(groupRow);
        groupRowLayout->setContentsMargins(0, 4, 0, 0);
        groupRowLayout->setSpacing(8);

        auto* groupLabel = new QLabel("Nhóm liên hệ");
        groupLabel->setObjectName("muted");
        groupLabel->setStyleSheet("font-size: 10px;");
        groupSelector_ = new QComboBox;
        groupSelector_->setMinimumWidth(120);
        groupSelector_->setStyleSheet(
            "QComboBox { background: #f8f9fc; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 7px 10px; color: #5955d6; "
            "font-size: 10px; }"
            "QComboBox:hover { background: #f0f1ff; }"
            "QComboBox::drop-down { border: none; width: 20px; }"
            "QComboBox::down-arrow { image: none; }"
            "QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 4px; outline: none; "
            "selection-background-color: #eef0ff; selection-color: #5955d6; }");

        auto* groupPopup = new QListView(groupSelector_);
        groupPopup->setIconSize(QSize(0, 0));
        groupPopup->setTextElideMode(Qt::ElideRight);
        groupPopup->setUniformItemSizes(true);
        groupPopup->setStyleSheet(
            "QListView { background: #ffffff; border: 1px solid #edf0f5; "
            "border-radius: 8px; padding: 4px; outline: none; }"
            "QListView::item { height: 28px; margin: 1px 0px; "
            "padding: 5px 8px; border-radius: 6px; color: #253047; }"
            "QListView::item:hover { background: #f2f3ff; color: #5955d6; }"
            "QListView::item:selected { background: #eef0ff; color: #5955d6; }");
        groupSelector_->setView(groupPopup);
        groupRowLayout->addWidget(groupLabel);
        groupRowLayout->addStretch();
        groupRowLayout->addWidget(groupSelector_);
        detailLayout_->addWidget(groupRow);

        cards->addWidget(listCard, 1);
        cards->addWidget(detailCard_);
        cardsWidget_ = new QWidget;
        cardsWidget_->setLayout(cards);
        contentLayout->addWidget(cardsWidget_, 1);

        //  Admin area is enabled only for admin sessions; its table omits passwords and contacts.
        adminFrame_ = new QFrame;
        adminFrame_->setObjectName("card");
        adminFrame_->setVisible(false);
        auto* adminLayout = new QVBoxLayout(adminFrame_);
        adminLayout->setContentsMargins(18, 18, 18, 18);
        adminLayout->setSpacing(12);

        auto* adminHeader = new QHBoxLayout;
        adminSearch_ = new QLineEdit;
        adminSearch_->setPlaceholderText("Tìm theo tên hoặc ID tài khoản...");
        adminSearch_->setClearButtonEnabled(true);
        adminHeader->addWidget(adminSearch_, 1);
        adminCountLabel_ = new QLabel("0 tài khoản");
        adminCountLabel_->setObjectName("muted");
        adminHeader->addWidget(adminCountLabel_);
        adminLayout->addLayout(adminHeader);

        adminTable_ = new QTableWidget;
        adminTable_->setColumnCount(4);
        adminTable_->setHorizontalHeaderLabels({"ID", "TÊN TÀI KHOẢN", "TRẠNG THÁI", "THAO TÁC"});
        adminTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        adminTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        adminTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        adminTable_->setShowGrid(false);
        adminTable_->verticalHeader()->setVisible(false);
        adminTable_->horizontalHeader()->setStretchLastSection(true);
        adminTable_->setAlternatingRowColors(true);
        adminLayout->addWidget(adminTable_, 1);
        contentLayout->addWidget(adminFrame_, 1);

        root->addWidget(sidebar);
        root->addWidget(content, 1);

        //  Connects UI events to loading/filtering functions and business actions.
        connect(search_, &QLineEdit::textChanged, this, &MainWindow::reload);
        connect(adminSearch_, &QLineEdit::textChanged, this, &MainWindow::reloadAdminUsers);
        connect(managementButton_, &QPushButton::clicked, this, &MainWindow::showAdminManagement);
        connect(allContactsButton_, &QPushButton::clicked, this, &MainWindow::showAllContacts);
        connect(favoritesButton_, &QPushButton::clicked, this, &MainWindow::showFavorites);
        connect(addGroupButton_, &QPushButton::clicked, this, &MainWindow::addGroup);
        connect(addButton_, &QPushButton::clicked, this, &MainWindow::addContact);
        connect(importButton_, &QPushButton::clicked, this, &MainWindow::importCsv);
        connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportCsv);
        connect(sideImportButton_, &QPushButton::clicked, this, &MainWindow::importCsv);
        connect(sideExportButton_, &QPushButton::clicked, this, &MainWindow::exportCsv);
        connect(editDetailButton_, &QPushButton::clicked, this, &MainWindow::editContact);
        connect(favoriteDetailButton_, &QPushButton::clicked,
                this, &MainWindow::toggleFavorite);
        connect(deleteDetailButton_, &QPushButton::clicked, this, &MainWindow::deleteContact);
        connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, &MainWindow::updateDetail);
        //  Clicking the star column toggles favorite except in the read-only favorites mode.
        connect(table_, &QTableView::clicked, this, [this](const QModelIndex& index) {
            if (index.isValid() && index.column() == 8 && !favoriteSelectionMode_)
                toggleFavoriteAt(index);
        });
        connect(groupSelector_, &QComboBox::activated,
                this, &MainWindow::changeContactGroup);

        table_->selectRow(0);
        updateDetail(table_->currentIndex(), QModelIndex());
        refreshGroups();
    }

private:
    // Session state, filter state, and pointers to the UI components.
    PhoneBookRuntime& runtime_;
    int userId_;
    int filterMode_ = 0;
    int selectedGroupId_ = 0;
    bool favoriteSelectionMode_ = false;
    bool isAdmin_ = false;
    int nameSortMode_ = 0;
    QLabel* title_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QLabel* footer_ = nullptr;
    QLabel* detailAvatar_ = nullptr;
    QLabel* detailName_ = nullptr;
    QLineEdit* search_ = nullptr;
    QTableView* table_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QPushButton* sideImportButton_ = nullptr;
    QPushButton* sideExportButton_ = nullptr;
    QPushButton* allContactsButton_ = nullptr;
    QPushButton* favoritesButton_ = nullptr;
    QPushButton* addGroupButton_ = nullptr;
    QPushButton* sortButton_ = nullptr;
    QPushButton* managementButton_ = nullptr;
    QFrame* listCard_ = nullptr;
    QFrame* adminFrame_ = nullptr;
    QWidget* cardsWidget_ = nullptr;
    QLineEdit* adminSearch_ = nullptr;
    QLabel* adminCountLabel_ = nullptr;
    QTableWidget* adminTable_ = nullptr;
    QHBoxLayout* titleRow_ = nullptr;
    QComboBox* groupFilter_ = nullptr;
    QWidget* groupListWidget_ = nullptr;
    QVBoxLayout* groupListLayout_ = nullptr;
    QFrame* detailCard_ = nullptr;
    QVBoxLayout* detailLayout_ = nullptr;
    QHBoxLayout* detailActions_ = nullptr;
    QPushButton* editDetailButton_ = nullptr;
    QPushButton* favoriteDetailButton_ = nullptr;
    QPushButton* deleteDetailButton_ = nullptr;
    QComboBox* groupSelector_ = nullptr;
    QList<QFrame*> detailRows_;

    //  Builds one detail row with an icon, label, and value.
    QFrame* detailLine(const char* iconName, const QString& label, const QString& value) {
        auto* frame = new QFrame;
        auto* row = new QHBoxLayout(frame);
        row->setContentsMargins(0, 3, 0, 3);
        row->setSpacing(10);

        auto* iconLabel = new QLabel;
        iconLabel->setPixmap(icon(iconName).pixmap(18, 18));
        iconLabel->setFixedWidth(22);

        auto* text = new QWidget;
        auto* textLayout = new QVBoxLayout(text);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        auto* title = new QLabel(label);
        title->setObjectName("muted");
        title->setStyleSheet("font-size: 10px;");
        auto* valueLabel = new QLabel(value);
        valueLabel->setObjectName("detailValue");
        valueLabel->setWordWrap(true);
        textLayout->addWidget(title);
        textLayout->addWidget(valueLabel);

        row->addWidget(iconLabel);
        row->addWidget(text, 1);
        detailRows_.append(frame);
        return frame;
    }

    //  Updates the value displayed in one detail-card row.
    void setDetailValue(int index, const QString& value) {
        if (index < 0 || index >= detailRows_.size())
            return;
        auto* valueLabel = detailRows_[index]->findChild<QLabel*>("detailValue");
        if (valueLabel)
            valueLabel->setText(value.isEmpty() ? "Chưa cập nhật" : value);
    }

    // Creates the initials displayed in the contact avatar.
    QString initials(const QString& name) {
        QStringList parts = name.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            return "?";
        if (parts.size() == 1)
            return parts.first().left(1).toUpper();
        return parts.first().left(1).toUpper() + parts.last().left(1).toUpper();
    }

    //  Gets filtered/sorted contacts from ContactService and populates the UI model.
    void reload() {
        QString text = search_->text().trimmed();
        const int groupId = filterMode_ == 2 ? selectedGroupId_ : 0;
        const bool favoritesOnly = filterMode_ == 1;
        const ContactSort sort = nameSortMode_ == 0
            ? ContactSort::LastNameAsc : ContactSort::FirstNameAsc;
        model_->removeRows(0, model_->rowCount());
        std::vector<Contact> contacts;
        try {
            contacts = runtime_.listContacts(
                userId_, text.toStdString(), groupId, favoritesOnly, sort);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể tải liên hệ",
                                  QString::fromUtf8(error.what()));
            return;
        }

        for (const Contact& contact : contacts) {
            const QString phone = contact.phones.empty()
                ? QString() : QString::fromStdString(contact.phones.front().number);
            const QString email = contact.emails.empty()
                ? QString() : QString::fromStdString(contact.emails.front().email);
            const QStringList values = {
                QString::number(contact.idContact),
                QString::number(contact.userId),
                QString::fromStdString(contact.fullname),
                phone,
                email,
                QString::fromStdString(contact.company),
                QString::fromStdString(contact.address),
                QString::fromStdString(contact.note),
                contact.isFavorite ? "1" : "0",
                QString::fromStdString(contact.lastname),
                QString::fromStdString(contact.firstname)
            };
            QList<QStandardItem*> items;
            for (int column = 0; column < values.size(); ++column) {
                const QString& value = values[column];
                auto* item = new QStandardItem(value);
                item->setEditable(false);
                if (column == 8)
                    item->setData(contact.isFavorite, Qt::DisplayRole);
                items.append(item);
            }
            model_->appendRow(items);
        }

        const int rows = model_->rowCount();
        countLabel_->setText(QString::number(rows));
        footer_->setText(QString("Hiển thị %1 liên hệ").arg(rows));
        if (rows > 0) {
            table_->selectRow(0);
            updateDetail(table_->currentIndex(), QModelIndex());
        } else {
            detailName_->setText("Chưa chọn liên hệ");
            detailAvatar_->setText("--");
            setDetailValue(0, "");
            setDetailValue(1, "");
            setDetailValue(2, "");
            updateFavoriteButton(false);
            groupSelector_->blockSignals(true);
            groupSelector_->clear();
            groupSelector_->addItem("Chưa chọn", 0);
            groupSelector_->blockSignals(false);
            groupSelector_->setEnabled(false);
        }
    }

    //  Clears the selected state from navigation items.
    void clearNavSelection() {
        allContactsButton_->setObjectName("");
        favoritesButton_->setObjectName("");
        if (managementButton_) {
            managementButton_->setObjectName("");
            managementButton_->style()->unpolish(managementButton_);
            managementButton_->style()->polish(managementButton_);
        }
        allContactsButton_->style()->unpolish(allContactsButton_);
        allContactsButton_->style()->polish(allContactsButton_);
        favoritesButton_->style()->unpolish(favoritesButton_);
        favoritesButton_->style()->polish(favoritesButton_);
        for (QPushButton* button : groupListWidget_->findChildren<QPushButton*>()) {
            button->setObjectName("");
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }

    //  Favorite mode disables edit/delete and permits changing only the star state.
    void setFavoriteSelectionMode(bool enabled) {
        favoriteSelectionMode_ = enabled;
        editDetailButton_->setEnabled(!enabled);
        deleteDetailButton_->setEnabled(!enabled);
        favoriteDetailButton_->setEnabled(!enabled);
        groupSelector_->setEnabled(!enabled);
        if (table_)
            table_->viewport()->update();
    }

    //  Restores the normal contacts layout and hides the admin area.
    void showNormalView() {
        if (cardsWidget_)
            cardsWidget_->setVisible(true);
        if (adminFrame_)
            adminFrame_->setVisible(false);
        if (listCard_)
            listCard_->setVisible(true);
        if (detailCard_)
            detailCard_->setVisible(true);
        if (countLabel_)
            countLabel_->setVisible(true);
        if (importButton_)
            importButton_->setVisible(true);
        if (exportButton_)
            exportButton_->setVisible(true);
        if (addButton_)
            addButton_->setVisible(true);
    }

    //   Opens account management only when the current session is an admin.
    void showAdminManagement() {
        if (!isAdmin_)
            return;
        if (cardsWidget_)
            cardsWidget_->setVisible(false);
        clearNavSelection();
        managementButton_->setObjectName("navActive");
        managementButton_->style()->unpolish(managementButton_);
        managementButton_->style()->polish(managementButton_);
        title_->setText("Quản trị tài khoản");
        if (listCard_)
            listCard_->setVisible(false);
        if (detailCard_)
            detailCard_->setVisible(false);
        if (adminFrame_)
            adminFrame_->setVisible(true);
        if (countLabel_)
            countLabel_->setVisible(false);
        if (importButton_)
            importButton_->setVisible(false);
        if (exportButton_)
            exportButton_->setVisible(false);
        if (addButton_)
            addButton_->setVisible(false);
        reloadAdminUsers();
    }

    //  Locks or unlocks an account through AdminService and refreshes the admin table.
    void setUserActive(int targetUserId, bool active) {
        try {
            runtime_.setUserActive(userId_, targetUserId, active);
            reloadAdminUsers();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể cập nhật tài khoản",
                                  QString::fromUtf8(error.what()));
        }
    }

    //  Admin issues a one-time token; it is shown only in a private dialog and never logged.
    void issueResetToken(int targetUserId) {
        if (QMessageBox::question(
                this, "Cấp mã đặt lại",
                "Tạo mã một lần cho tài khoản này? Mã sẽ hết hạn sau 15 phút.",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        try {
            const QString token = QString::fromStdString(
                runtime_.issuePasswordResetToken(userId_, targetUserId));
            QDialog dialog(this);
            dialog.setWindowTitle("Mã đặt lại mật khẩu");
            dialog.setModal(true);
            auto* layout = new QVBoxLayout(&dialog);
            layout->addWidget(new QLabel(
                "Gửi mã riêng cho người dùng. Mã chỉ dùng một lần và không được ghi vào log."));
            auto* tokenField = new QLineEdit(token);
            tokenField->setReadOnly(true);
            tokenField->setEchoMode(QLineEdit::Password);
            layout->addWidget(tokenField);
            auto* reveal = new QCheckBox("Hiện mã");
            layout->addWidget(reveal);
            auto* buttons = new QHBoxLayout;
            auto* copy = new QPushButton("Sao chép mã");
            auto* close = new QPushButton("Đóng");
            buttons->addStretch();
            buttons->addWidget(copy);
            buttons->addWidget(close);
            layout->addLayout(buttons);
            connect(reveal, &QCheckBox::toggled, tokenField,
                    [tokenField](bool visible) {
                        tokenField->setEchoMode(
                            visible ? QLineEdit::Normal : QLineEdit::Password);
                    });
            connect(copy, &QPushButton::clicked, this, [token] {
                QApplication::clipboard()->setText(token);
            });
            connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
            dialog.exec();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể cấp mã",
                                  QString::fromUtf8(error.what()));
        }
    }

    // Searches and displays IDs, names, statuses, and permitted admin actions.
    void reloadAdminUsers() {
        if (!adminTable_)
            return;
        try {
            const auto users = runtime_.listUsers(
                userId_, adminSearch_->text().trimmed().toStdString());
            adminTable_->setRowCount(static_cast<int>(users.size()));
            for (int row = 0; row < static_cast<int>(users.size()); ++row) {
                const RuntimeUser& user = users[static_cast<size_t>(row)];
                adminTable_->setItem(
                    row, 0, new QTableWidgetItem(QString::number(user.id)));
                adminTable_->setItem(
                    row, 1, new QTableWidgetItem(QString::fromStdString(user.name)));
                adminTable_->setItem(
                    row, 2, new QTableWidgetItem(user.active ? "Hoạt động" : "Đã khóa"));

                auto* actions = new QWidget;
                auto* actionsLayout = new QHBoxLayout(actions);
                actionsLayout->setContentsMargins(4, 2, 4, 2);
                actionsLayout->setSpacing(6);
                auto* lockButton = new QPushButton("Khóa");
                auto* unlockButton = new QPushButton("Mở");
                auto* resetButton = new QPushButton("Mã");
                lockButton->setEnabled(user.active);
                unlockButton->setEnabled(!user.active);
                resetButton->setEnabled(user.active);
                resetButton->setToolTip("Cấp mã đặt lại mật khẩu dùng một lần");
                lockButton->setStyleSheet(
                    "QPushButton { background:#fff0f0; color:#c44a4a; border:none; "
                    "border-radius:6px; padding:5px 8px; }");
                unlockButton->setStyleSheet(
                    "QPushButton { background:#edf7ef; color:#3d8a4c; border:none; "
                    "border-radius:6px; padding:5px 8px; }");
                actionsLayout->addWidget(lockButton);
                actionsLayout->addWidget(unlockButton);
                actionsLayout->addWidget(resetButton);
                adminTable_->setCellWidget(row, 3, actions);

                connect(lockButton, &QPushButton::clicked, this,
                        [this, userId = user.id] { setUserActive(userId, false); });
                connect(unlockButton, &QPushButton::clicked, this,
                        [this, userId = user.id] { setUserActive(userId, true); });
                connect(resetButton, &QPushButton::clicked, this,
                        [this, userId = user.id] { issueResetToken(userId); });
            }
            adminCountLabel_->setText(
                QString("%1 tài khoản").arg(runtime_.countUsers(userId_)));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể tải tài khoản",
                                  QString::fromUtf8(error.what()));
        }
    }

    //  Shows all contacts and resets the filters.
    void showAllContacts() {
        clearNavSelection();
        showNormalView();
        allContactsButton_->setObjectName("navActive");
        allContactsButton_->style()->unpolish(allContactsButton_);
        allContactsButton_->style()->polish(allContactsButton_);
        setFavoriteSelectionMode(false);
        if (groupFilter_)
            groupFilter_->setCurrentIndex(0);
        filterMode_ = 0;
        selectedGroupId_ = 0;
        title_->setText("Tất cả liên hệ");
        reload();
    }

    //  Enables the favorites filter; edit/delete actions are disabled in this mode.
    void showFavorites() {
        clearNavSelection();
        showNormalView();
        favoritesButton_->setObjectName("navActive");
        favoritesButton_->style()->unpolish(favoritesButton_);
        favoritesButton_->style()->polish(favoritesButton_);
        setFavoriteSelectionMode(true);
        filterMode_ = 1;
        selectedGroupId_ = 0;
        title_->setText("Yêu thích");
        reload();
    }

    //  Selects a contact group and displays only contacts assigned to it.
    void showGroup(int groupId, const QString& groupName, QPushButton* sourceButton) {
        clearNavSelection();
        showNormalView();
        sourceButton->setObjectName("navActive");
        sourceButton->style()->unpolish(sourceButton);
        sourceButton->style()->polish(sourceButton);
        setFavoriteSelectionMode(false);
        if (groupFilter_) {
            int index = groupFilter_->findData(groupId);
            if (index >= 0)
                groupFilter_->setCurrentIndex(index);
        }
        filterMode_ = 2;
        selectedGroupId_ = groupId;
        title_->setText(groupName);
        reload();
    }

    //  Loads groups from GroupService and builds sidebar group/delete buttons.
    void refreshGroups() {
        while (auto* item = groupListLayout_->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }

        if (groupFilter_) {
            while (groupFilter_->count() > 1)
                groupFilter_->removeItem(groupFilter_->count() - 1);
        }

        for (const RuntimeGroup& group : runtime_.listGroups(userId_)) {
            int groupId = group.id;
            QString groupName = QString::fromStdString(group.name);
            auto* groupRow = new QWidget;
            auto* groupRowLayout = new QHBoxLayout(groupRow);
            groupRowLayout->setContentsMargins(0, 0, 0, 0);
            groupRowLayout->setSpacing(2);

            auto* button = iconButton("users", "  " + groupName);
            button->setProperty("groupId", groupId);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            groupRowLayout->addWidget(button);

            auto* deleteButton = iconButton("trash", "");
            deleteButton->setFixedSize(28, 28);
            deleteButton->setIconSize(QSize(15, 15));
            deleteButton->setToolTip("Xóa nhóm liên hệ");
            deleteButton->setStyleSheet(
                "QPushButton { border: none; border-radius: 6px; "
                "background: transparent; padding: 6px; }"
                "QPushButton:hover { background: #fff0f0; }");
            groupRowLayout->addWidget(deleteButton);

            groupListLayout_->addWidget(groupRow);
            if (groupFilter_)
                groupFilter_->addItem(groupName, groupId);
            connect(button, &QPushButton::clicked, this,
                    [this, groupId, groupName, button] {
                        showGroup(groupId, groupName, button);
                    });
            connect(deleteButton, &QPushButton::clicked, this,
                    [this, groupId, groupName] {
                        deleteGroup(groupId, groupName);
                    });
        }
    }

    // Deletes a group after confirmation, then refreshes filters and the contact list.
    void deleteGroup(int groupId, const QString& groupName) {
        if (QMessageBox::question(
                this,
                "Xóa nhóm liên hệ",
                QString("Bạn có chắc muốn xóa nhóm \"%1\"?").arg(groupName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
            return;
        }

        try {
            runtime_.removeGroup(userId_, groupId);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể xóa nhóm",
                                  QString::fromUtf8(error.what()));
            return;
        }

        if (selectedGroupId_ == groupId)
            showAllContacts();
        refreshGroups();
        reload();
    }

    //  Shows the create-group form; GroupService validates and persists the group.
    void addGroup() {
        QDialog dialog(this);
        dialog.setWindowTitle("Thêm nhóm liên hệ");
        dialog.setModal(true);
        dialog.resize(520, 300);
        dialog.setStyleSheet(
            "QDialog { background: #ffffff; }"
            "QLabel#groupDialogTitle { color: #253047; font-size: 16px; font-weight: 700; }"
            "QLabel#groupDialogSubtitle { color: #98a0b3; font-size: 11px; }"
            "QLabel#groupFieldLabel { color: #253047; font-size: 11px; font-weight: 650; }"
            "QLabel#groupRequiredNote { color: #98a0b3; font-size: 10px; }"
            "QLineEdit { border: 1px solid #e2e5ef; border-radius: 8px; "
            "background: #ffffff; color: #253047; padding: 8px 10px 8px 34px; "
            "selection-background-color: #eef0ff; }"
            "QPushButton#groupDialogClose { border: none; background: transparent; padding: 4px; }"
            "QPushButton#groupDialogSecondary { background: #f2f3ff; color: #5955d6; "
            "border: none; border-radius: 8px; padding: 10px 18px; }"
            "QPushButton#groupDialogPrimary { background: #5955d6; color: #ffffff; "
            "border: none; border-radius: 8px; padding: 10px 18px; font-weight: 650; }");

        auto* root = new QVBoxLayout(&dialog);
        root->setContentsMargins(28, 22, 28, 20);
        root->setSpacing(10);

        auto* header = new QHBoxLayout;
        auto* headerIcon = new QLabel;
        headerIcon->setPixmap(icon("users").pixmap(22, 22));
        headerIcon->setAlignment(Qt::AlignCenter);
        headerIcon->setFixedSize(38, 38);
        headerIcon->setStyleSheet("background: #eef0ff; border-radius: 19px;");

        auto* headerText = new QWidget;
        auto* headerTextLayout = new QVBoxLayout(headerText);
        headerTextLayout->setContentsMargins(0, 0, 0, 0);
        headerTextLayout->setSpacing(2);
        auto* title = new QLabel("Thêm nhóm liên hệ");
        title->setObjectName("groupDialogTitle");
        auto* subtitle = new QLabel("Tạo nhóm để dễ dàng phân loại liên hệ.");
        subtitle->setObjectName("groupDialogSubtitle");
        headerTextLayout->addWidget(title);
        headerTextLayout->addWidget(subtitle);

        auto* closeButton = new QPushButton;
        closeButton->setObjectName("groupDialogClose");
        closeButton->setIcon(icon("dialog-x"));
        closeButton->setIconSize(QSize(18, 18));
        closeButton->setFixedSize(30, 30);
        closeButton->setCursor(Qt::PointingHandCursor);
        header->addWidget(headerIcon);
        header->addWidget(headerText, 1);
        header->addWidget(closeButton, 0, Qt::AlignTop);
        root->addLayout(header);
        QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);

        auto* fieldLabel = new QLabel("Tên nhóm *");
        fieldLabel->setObjectName("groupFieldLabel");
        root->addWidget(fieldLabel);

        auto* nameEdit = new QLineEdit;
        nameEdit->setPlaceholderText("Nhập tên nhóm liên hệ");
        nameEdit->setFixedHeight(42);
        nameEdit->addAction(new QAction(icon("users"), {}, nameEdit),
                            QLineEdit::LeadingPosition);
        root->addWidget(nameEdit);

        auto* required = new QLabel("* Thông tin bắt buộc");
        required->setObjectName("groupRequiredNote");
        root->addWidget(required);
        root->addStretch();

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancelButton = new QPushButton("Hủy");
        cancelButton->setObjectName("groupDialogSecondary");
        cancelButton->setCursor(Qt::PointingHandCursor);
        auto* saveButton = new QPushButton;
        saveButton->setText("  Tạo nhóm");
        saveButton->setObjectName("groupDialogPrimary");
        saveButton->setIcon(icon("dialog-check"));
        saveButton->setIconSize(QSize(16, 16));
        saveButton->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(cancelButton);
        buttons->addWidget(saveButton);
        root->addLayout(buttons);

        QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
        nameEdit->setFocus();

        QString name;
        QObject::connect(saveButton, &QPushButton::clicked, &dialog, [&dialog, &name, nameEdit, this] {
            name = nameEdit->text().trimmed();
            if (name.isEmpty()) {
                QMessageBox::warning(&dialog, "Tên nhóm trống",
                                     "Vui lòng nhập tên nhóm liên hệ.");
                return;
            }
            dialog.accept();
        });

        if (dialog.exec() != QDialog::Accepted)
            return;

        try {
            runtime_.createGroup(userId_, name.toStdString());
        } catch (const std::exception& error) {
            QMessageBox::warning(this, "Không thể thêm nhóm",
                                 QString::fromUtf8(error.what()));
            return;
        }

        refreshGroups();
        QMessageBox::information(this, "Thêm nhóm thành công",
                                 QString("Đã tạo nhóm \"%1\".").arg(name));
    }

    // Synchronizes the selected row's name, avatar, and fields into the detail card.
    void updateDetail(const QModelIndex& current, const QModelIndex&) {
        if (!current.isValid()) {
            detailName_->setText("Chưa chọn liên hệ");
            detailAvatar_->setText("--");
            setDetailValue(0, "");
            setDetailValue(1, "");
            setDetailValue(2, "");
            favoriteDetailButton_->setText("  Yêu thích");
            favoriteDetailButton_->setIcon(icon("star"));
            groupSelector_->clear();
            groupSelector_->addItem("Chưa chọn", 0);
            return;
        }
        int row = current.row();
        int contactId = model_->index(row, 0).data().toInt();
        QString name = model_->index(row, 2).data().toString();
        detailName_->setText(name);
        detailAvatar_->setText(initials(name));
        setDetailValue(0, model_->index(row, 3).data().toString());
        setDetailValue(1, model_->index(row, 4).data().toString());
        setDetailValue(2, model_->index(row, 6).data().toString());
        bool isFavorite = model_->index(row, 8).data().toBool();
        favoriteDetailButton_->setText(isFavorite ? "  Đã yêu thích" : "  Yêu thích");
        favoriteDetailButton_->setIcon(icon(isFavorite ? "star" : "star"));
        refreshContactGroupSelector(contactId);
    }

    // Loads the selected contact's current group and available group choices.
    void refreshContactGroupSelector(int contactId) {
        groupSelector_->blockSignals(true);
        groupSelector_->clear();
        groupSelector_->addItem("Chưa chọn", 0);

        int currentGroupId = 0;
        const auto contactGroups = runtime_.contactGroupIds(userId_, contactId);
        if (!contactGroups.empty())
            currentGroupId = contactGroups.front();
        for (const RuntimeGroup& group : runtime_.listGroups(userId_)) {
            groupSelector_->addItem(QString::fromStdString(group.name), group.id);
            if (group.id == currentGroupId)
                groupSelector_->setCurrentIndex(groupSelector_->count() - 1);
        }
        groupSelector_->blockSignals(false);
        groupSelector_->setEnabled(!favoriteSelectionMode_ && contactId > 0);
    }

    //  Saves the selected group through the runtime and refreshes a group-filtered list.
    void changeContactGroup(int index) {
        QModelIndex contactIndex = table_->currentIndex();
        if (!contactIndex.isValid() || index < 0)
            return;

        int contactId = model_->index(contactIndex.row(), 0).data().toInt();
        int groupId = groupSelector_->itemData(index).toInt();

        try {
            runtime_.setContactGroup(userId_, contactId, groupId);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể cập nhật nhóm",
                                  QString::fromUtf8(error.what()));
            return;
        }

        if (filterMode_ == 2)
            reload();
    }

    //  Toggles the favorite state of the currently selected contact.
    void toggleFavorite() {
        QModelIndex index = table_->currentIndex();
        if (!index.isValid()) {
            QMessageBox::information(this, "Chưa chọn liên hệ",
                                     "Vui lòng chọn liên hệ cần đánh dấu.");
            return;
        }
        toggleFavoriteAt(index);
    }

    // Persists the star state through the service and reloads the filtered results.
    void toggleFavoriteAt(const QModelIndex& index) {
        int row = index.row();
        int contactId = model_->index(row, 0).data().toInt();
        try {
            runtime_.toggleFavorite(userId_, contactId);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể cập nhật yêu thích",
                                  QString::fromUtf8(error.what()));
            return;
        }
        reload();
    }

    // Updates the favorite button label and icon to match the contact state.
    void updateFavoriteButton(bool isFavorite) {
        favoriteDetailButton_->setText(isFavorite ? "  Đã yêu thích" : "  Yêu thích");
        favoriteDetailButton_->setIcon(icon("star"));
    }

    void importCsv() {
        //  Qt selects the file; CsvService parses it, checks duplicates, and saves contacts.
        QString path = QFileDialog::getOpenFileName(
            this, "Nhập danh bạ từ CSV", QString(), "CSV (*.csv);;Mọi tệp (*)");
        if (path.isEmpty())
            return;
        try {
            const ImportResult result =
                runtime_.importContacts(userId_, path.toStdString());
            reload();
            QString message = QString("Đã nhập %1 liên hệ.\nBỏ qua %2 dòng.")
                .arg(result.imported).arg(result.skipped);
            if (!result.errors.empty()) {
                message += "\n\n";
                for (size_t i = 0; i < std::min<size_t>(result.errors.size(), 5); ++i)
                    message += QString::fromStdString(result.errors[i]) + "\n";
            }
            QMessageBox::information(this, "Nhập CSV hoàn tất", message);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể nhập CSV",
                                  QString::fromUtf8(error.what()));
        }
    }

    void exportCsv() {
        //  CsvService reads and serializes data; the UI only chooses the output path.
        QString path = QFileDialog::getSaveFileName(
            this, "Xuất danh bạ ra CSV", "phonebook.csv", "CSV (*.csv)");
        if (path.isEmpty())
            return;
        if (!path.endsWith(".csv", Qt::CaseInsensitive))
            path += ".csv";

        try {
            const int exported =
                runtime_.exportContacts(userId_, path.toStdString());
            QMessageBox::information(
                this, "Xuất CSV hoàn tất",
                QString("Đã xuất %1 liên hệ ra:\n%2").arg(exported).arg(path));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể xuất CSV",
                                  QString::fromUtf8(error.what()));
        }
    }

    // Collects ContactDialog values and asks ContactService to create the contact.
    void addContact() {
        ContactDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        RuntimeContact contact;
        contact.lastName = dialog.lastname_->text().trimmed().toStdString();
        contact.firstName = dialog.firstname_->text().trimmed().toStdString();
        contact.phone = dialog.phone_->text().trimmed().toStdString();
        contact.email = dialog.email_->text().trimmed().toStdString();
        contact.company = dialog.company_->text().trimmed().toStdString();
        contact.address = dialog.address_->text().trimmed().toStdString();
        contact.note = dialog.note_->toPlainText().trimmed().toStdString();
        try {
            runtime_.addContact(userId_, contact);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể thêm liên hệ",
                                  QString::fromUtf8(error.what()));
            return;
        }
        reload();
    }

    //  Loads a contact into the form, submits edits through the service, then reselects its row.
    void editContact() {
        if (favoriteSelectionMode_)
            return;
        QModelIndex index = table_->currentIndex();
        if (!index.isValid())
            return;
        int row = index.row();
        ContactDialog dialog(this);
        dialog.setWindowTitle("Sửa liên hệ");
        QString fullname = model_->index(row, 2).data().toString();
        QString lastname = model_->index(row, 9).data().toString();
        QString firstname = model_->index(row, 10).data().toString();
        if (lastname.isEmpty() || firstname.isEmpty()) {
            QStringList parts = fullname.split(' ', Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                lastname = parts.takeFirst();
                firstname = parts.join(' ');
            }
        }
        dialog.lastname_->setText(lastname);
        dialog.firstname_->setText(firstname);
        dialog.phone_->setText(model_->index(row, 3).data().toString());
        dialog.email_->setText(model_->index(row, 4).data().toString());
        dialog.company_->setText(model_->index(row, 5).data().toString());
        dialog.address_->setText(model_->index(row, 6).data().toString());
        dialog.note_->setPlainText(model_->index(row, 7).data().toString());
        if (dialog.exec() != QDialog::Accepted)
            return;

        RuntimeContact contact;
        contact.lastName = dialog.lastname_->text().trimmed().toStdString();
        contact.firstName = dialog.firstname_->text().trimmed().toStdString();
        contact.phone = dialog.phone_->text().trimmed().toStdString();
        contact.email = dialog.email_->text().trimmed().toStdString();
        contact.company = dialog.company_->text().trimmed().toStdString();
        contact.address = dialog.address_->text().trimmed().toStdString();
        contact.note = dialog.note_->toPlainText().trimmed().toStdString();
        int contactId = model_->index(row, 0).data().toInt();
        try {
            runtime_.updateContact(userId_, contactId, contact);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Không thể cập nhật",
                                  QString::fromUtf8(error.what()));
            return;
        }
        reload();
        for (int updatedRow = 0; updatedRow < model_->rowCount(); ++updatedRow) {
            if (model_->index(updatedRow, 0).data().toInt() == contactId) {
                table_->selectRow(updatedRow);
                updateDetail(table_->currentIndex(), QModelIndex());
                break;
            }
        }
    }

    //  Confirms contact deletion and performs it through ContactService.
    void deleteContact() {
        if (favoriteSelectionMode_)
            return;
        QModelIndex index = table_->currentIndex();
        if (!index.isValid())
            return;
        QString name = model_->index(index.row(), 2).data().toString();
        if (QMessageBox::question(this, "Xóa liên hệ",
                                  QString("Bạn muốn xóa \"%1\"?").arg(name)) == QMessageBox::Yes) {
            int contactId = model_->index(index.row(), 0).data().toInt();
            try {
                runtime_.removeContact(userId_, contactId);
                reload();
            } catch (const std::exception& error) {
                QMessageBox::critical(this, "Không thể xóa",
                                      QString::fromUtf8(error.what()));
            }
        }
    }
};

} // namespace

int main(int argc, char* argv[]) {
    // Application entry point; creates QApplication and resolves the database path.
    QApplication app(argc, argv);
    app.setApplicationName("Phone Book Management");
    app.setStyleSheet(
        "QMessageBox { background-color: #ffffff; color: #000000; }"
        "QMessageBox QLabel { background-color: transparent; color: #000000; }"
        "QMessageBox QPushButton { color: #000000; background: #f2f3ff; "
        "border: 1px solid #dfe3ec; border-radius: 4px; padding: 5px 16px; }"
        "QMessageBox QPushButton:hover { background: #eef0ff; }");

    QString dataDirectory = QDir::cleanPath(
        QApplication::applicationDirPath() + "/../data");
    QDir().mkpath(dataDirectory);
    QString databasePath = dataDirectory + "/phonebook.db";
    std::unique_ptr<PhoneBookRuntime> runtime;
    try {
        // Runtime initializes the schema, optional sample data, migrations, and services.
        runtime = std::make_unique<PhoneBookRuntime>(databasePath.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "Lỗi khởi tạo runtime",
                              QString::fromUtf8(error.what()));
        return 1;
    }

    //  Requires login before creating the contacts window.
    LoginDialog login(*runtime);
    if (login.exec() != QDialog::Accepted)
        return 0;

    MainWindow window(*runtime, login.userId(), login.isAdmin());
    window.show();
    return app.exec();
}
