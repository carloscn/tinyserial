#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "appversion.h"
#include <QDialogButtonBox>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    setLanguage(UiLanguage::English);
}

void AboutDialog::setLanguage(UiLanguage language)
{
    setWindowTitle(uiText(language, "about_tinyserial"));
    ui->label->setText(QStringLiteral("<b>TinySerial</b>"));
    ui->label_7->setText(uiText(language, "about_subtitle"));
    ui->label_8->setText(uiText(language, "version_value").arg(QLatin1String(APP_VERSION)));
    ui->label_5->setText(QStringLiteral("Qt %1").arg(QLatin1String(QT_VERSION_STR)));
    ui->label_9->setText(QStringLiteral("Copyright © 2014–2026 MULTIBEANS"));
    ui->label_6->setText(uiText(language, "about_warranty"));
    if (QPushButton *closeButton = ui->buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setText(uiText(language, "close"));
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
