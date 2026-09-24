#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "appversion.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->label_8->setText(QStringLiteral("Version v") + QLatin1String(APP_VERSION));
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
