// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/c_note/settings/settingsfaqwidget.h"
#include "qt/c_note/qtutils.h"
#include "qt/c_note/settings/forms/ui_settingsfaqwidget.h"
#include <QMetaObject>
#include <QScrollBar>

SettingsFaqWidget::SettingsFaqWidget(C_NoteGUI* parent) : QDialog(parent),
                                                                           ui(new Ui::SettingsFaqWidget)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

#ifdef Q_OS_MAC
    ui->container->load("://bg-welcome");
    setCssProperty(ui->container, "container-welcome-no-image");
#else
    setCssProperty(ui->container, "container-welcome");
#endif
    setCssProperty(ui->labelTitle, "text-title-faq");
    setCssProperty(ui->labelWebLink, "text-content-white");

    // Content
    setCssProperty({ui->labelNumber_Intro,
                       ui->labelNumber_UnspendableCNOTE,
                       ui->labelNumber_Stake,
                       ui->labelNumber_Support,
                       ui->labelNumber_Masternode,
                       ui->labelNumber_MNController},
        "container-number-faq");

    setCssProperty({ui->labelSubtitle_Intro,
                       ui->labelSubtitle_UnspendableCNOTE,
                       ui->labelSubtitle_Stake,
                       ui->labelSubtitle_Support,
                       ui->labelSubtitle_Masternode,
                       ui->labelSubtitle_MNController},
        "text-subtitle-faq");


    setCssProperty({ui->labelContent_Intro,
                       ui->labelContent_UnspendableCNOTE,
                       ui->labelContent_Stake,
                       ui->labelContent_Support,
                       ui->labelContent_Masternode,
                       ui->labelContent_MNController},
        "text-content-faq");


    setCssProperty({ui->pushButton_Intro,
                       ui->pushButton_UnspendableCNOTE,
                       ui->pushButton_Stake,
                       ui->pushButton_Support,
                       ui->pushButton_Masternode,
                       ui->pushButton_MNController},
        "btn-faq-options");

    ui->labelContent_Support->setOpenExternalLinks(true);

    // Set FAQ content strings
    QString introContent = formatFAQContent(
        formatFAQParagraph(
            tr("C-Note is a form of digital online money using blockchain technology "
               "that can be easily transferred globally, instantly, and with "
               "zero fees. C-Note incorporates market leading security & "
               "privacy features and supports the ZeroCoin (zCNOTE) protocol  "
               "for anonymous transactions through Zerocoin staking.")) +
        formatFAQParagraph(
            tr("More than just a privacy-focused coin, C-NOTE is a Gold Standard "
               "blockchain an innovative Proof-of-Stake (PoS) technology platform whose "
               "core mission is to provide enduring, interest-yielding cryptographic financial "
               "instruments known as C-Bonds. These instruments represent a new frontier in digital assets, "
               "combining the stability of gold-standard concepts with the flexibility and transparency of blockchain.")) +
        formatFAQParagraph(
            tr("C-NOTE utilizes a Proof-of-Stake consensus system, allowing all"
               "holders to earn block rewards while securing the network via full node wallets."
               "Additionally, users can run masternodes to participate"
               "in governance by creating and voting on proposals.")));
    ui->labelContent_Intro->setText(introContent);

    QString unspendableCNOTEContent = formatFAQContent(
        formatFAQParagraph(
            tr("Newly received C-Note requires 6 confirmations on the network "
               "to become eligible for spending which can take ~12-15 minutes.")) +
        formatFAQParagraph(
            tr("Your C-Note wallet also needs to be completely synchronized "
               "to see and spend balances on the network.")));
    ui->labelContent_UnspendableCNOTE->setText(unspendableCNOTEContent);

    QString stakeContent = formatFAQContent(
        formatFAQOrderedList(
            formatFAQListItem(tr("Make sure your wallet is completely synchronized and you are using the latest release.")) +
            formatFAQListItem(tr("You must have a balance of C-Note with a minimum of 60 confirmations.")) +
            formatFAQListItem(tr("Your wallet must stay online and be unlocked for staking purposes.")) +
            formatFAQListItem(tr("Once all those steps are followed staking should be enabled."))) +
        formatFAQParagraph(
            tr("You can see the status of staking in the wallet by mousing over the "
               "package icon in the row on the top left of the wallet interface. The "
               "package will be lit up and will state \"Staking Enabled\" to indicate "
               "it is staking. Using the command line interface (%1); the command %2 "
               "will confirm that staking is active.")
                .arg("c-note", "<span style=\"font-style:italic\">getstakingstatus</span>")));

    ui->labelContent_Stake->setText(stakeContent);

    QString supportContent = formatFAQContent(
        formatFAQParagraph(
            tr("We have support channels in most of our official chat groups, for example %1")
                .arg("<a style='color: #F3B307' href='https://discord.com/invite/zcu6xU4d'>" + tr("#support in our Discord") + "</a>.")));
    ui->labelContent_Support->setText(supportContent);

    QString masternodeContent = formatFAQContent(
        formatFAQParagraph(
            tr("A masternode is a computer running a full node C-Note wallet with a "
               "requirement of Collateral Amount of C-NOTE secured collateral to provide extra services "
               "to the network and in return, receive a portion of the block reward "
               "regularly. These services include:") +
            formatFAQUnorderedList(
                formatFAQListItem(tr("A decentralized governance (Proposal Voting)")) +
                formatFAQListItem(tr("A decentralized budgeting system (Treasury)")) +
                formatFAQListItem(tr("Validation of transactions within each block")) +
                formatFAQListItem(tr("Act as an additional full node in the network")))) +
        formatFAQParagraph(
            tr("For providing such services, masternodes are also paid a certain portion "
               "of reward for each block. This can serve as a passive income to the "
               "masternode owners minus their running cost.")) +
        formatFAQParagraph(
            tr("Masternode Perks:") +
            formatFAQUnorderedList(
                formatFAQListItem(tr("Participate in C-Note Governance")) +
                formatFAQListItem(tr("Earn Masternode Rewards")) +
                formatFAQListItem(tr("Commodity option for future sale")) +
                formatFAQListItem(tr("Help secure the C-Note network")))) +
        formatFAQParagraph(
            tr("Requirements:") +
            formatFAQUnorderedList(
                formatFAQListItem(tr("Collateral Amount of C-NOTE per single Masternode instance")) +
                formatFAQListItem(tr("Must be stored in a core wallet")) +
                formatFAQListItem(tr("Need dedicated IP address")) +
                formatFAQListItem(tr("Masternode wallet to remain online")))));
    ui->labelContent_Masternode->setText(masternodeContent);

    QString mNControllerContent = formatFAQContent(
        formatFAQParagraph(
            tr("A Masternode Controller wallet is where the Collateral Amount of C-NOTE collateral "
               "can reside during a Controller-Remote masternode setup. It is a wallet "
               "that can activate the remote masternode wallet(s) and allows you to keep "
               "your collateral coins offline while the remote masternode remains online.")));
    ui->labelContent_MNController->setText(mNControllerContent);


    // Exit button
    setCssProperty(ui->pushButtonExit, "btn-faq-exit");

    // Web Link
    setCssProperty(ui->pushButtonWebLink, "btn-faq-web");
    setCssProperty(ui->containerButtons, "container-faq-buttons");

    // Buttons
    connect(ui->pushButtonExit, &QPushButton::clicked, this, &SettingsFaqWidget::close);
    connect(ui->pushButton_Intro, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_Intro); });
    connect(ui->pushButton_UnspendableCNOTE, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_UnspendableCNOTE); });
    connect(ui->pushButton_Stake, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_Stake); });
    connect(ui->pushButton_Support, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_Support); });
    connect(ui->pushButton_Masternode, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_Masternode); });
    connect(ui->pushButton_MNController, &QPushButton::clicked, [this]() { onFaqClicked(ui->widget_MNController); });

    if (parent)
        connect(parent, &C_NoteGUI::windowResizeEvent, this, &SettingsFaqWidget::windowResizeEvent);
}

void SettingsFaqWidget::showEvent(QShowEvent* event)
{
    QPushButton* btn = getButtons()[section];
    QMetaObject::invokeMethod(btn, "setChecked", Qt::QueuedConnection, Q_ARG(bool, true));
    QMetaObject::invokeMethod(btn, "clicked", Qt::QueuedConnection);
}

void SettingsFaqWidget::setSection(Section _section)
{
    section = _section;
}

void SettingsFaqWidget::onFaqClicked(const QWidget* const widget)
{
    ui->scrollAreaFaq->verticalScrollBar()->setValue(widget->y());
}

void SettingsFaqWidget::windowResizeEvent(QResizeEvent* event)
{
    QWidget* w = qobject_cast<QWidget*>(parent());
    this->resize(w->width(), w->height());
    this->move(QPoint(0, 0));
}

std::vector<QPushButton*> SettingsFaqWidget::getButtons()
{
    return {
        ui->pushButton_Intro,
        ui->pushButton_UnspendableCNOTE,
        ui->pushButton_Stake,
        ui->pushButton_Support,
        ui->pushButton_Masternode,
        ui->pushButton_MNController};
}

SettingsFaqWidget::~SettingsFaqWidget()
{
    delete ui;
}
