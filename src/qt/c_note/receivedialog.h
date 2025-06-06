// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RECEIVEDIALOG_H
#define RECEIVEDIALOG_H

#include "qt/c_note/focuseddialog.h"
#include <QPixmap>

class SendCoinsRecipient;

namespace Ui {
class ReceiveDialog;
}

class ReceiveDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit ReceiveDialog(QWidget *parent = nullptr);
    ~ReceiveDialog();

    void updateQr(QString address);

private Q_SLOTS:
    void onCopy();
private:
    Ui::ReceiveDialog *ui{nullptr};
    QPixmap *qrImage{nullptr};
    SendCoinsRecipient *info{nullptr};
};

#endif // RECEIVEDIALOG_H
