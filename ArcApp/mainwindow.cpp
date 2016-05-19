#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "takephoto.h"
#include <QTableView>
#include <QItemDelegate>
#include <QStandardItemModel>
#include "payment.h"
#include <QPrinter>
#include <QProgressDialog>
#include <qtrpt.h>
#include "worker.h"

QLabel *lbl_curUser;
QLabel *lbl_curShift;

//MyModel* checkoutModel;
Report *checkoutReport, *vacancyReport, *lunchReport, *wakeupReport,
    *bookingReport, *transactionReport, *clientLogReport, *otherReport,
    *yellowReport, *redReport;
bool firstTime = true;
QStack<int> backStack;
QStack<int> forwardStack;
int workFlow;

//client search info
int transacNum, transacTotal;
bool newTrans;
int bookingNum, bookingTotal;
bool newHistory;
int maxClient;
QFuture<void> displayFuture ;
QFuture<void> displayPicFuture;
QFuture<void> transacFuture;
QFuture<void> bookHistoryFuture;

//register Type
int registerType;

//CaseFiles stuff
QVector<QTableWidget*> pcp_tables;
QVector<QString> pcpTypes;
bool loaded = false;
QString idDisplayed;


QProgressDialog* dialog;

//QSqlQuery resultssss;
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setCentralWidget(ui->stackedWidget);

    ui->makeBookingButton->hide();
    //mw = this;

    //default signal of stackedWidget
    //detect if the widget is changed
    connect(ui->stackedWidget, SIGNAL(currentChanged(int)), this, SLOT(initCurrentWidget(int)));
    connect(dbManager, SIGNAL(dailyReportStatsChanged(QList<int>, bool)), this,
        SLOT(updateDailyReportStats(QList<int>, bool)));
    connect(dbManager, SIGNAL(shiftReportStatsChanged(QStringList, bool)), this,
        SLOT(updateShiftReportStats(QStringList, bool)));
    connect(dbManager, SIGNAL(cashFloatChanged(QDate, int, QStringList, bool)), this,
        SLOT(updateCashFloat(QDate, int, QStringList, bool)));
    connect(dbManager, SIGNAL(cashFloatInserted(QString, QString, QString)), this,
        SLOT(updateCashFloatLastEditedLabels(QString, QString, QString)));
    connect(ui->other_lineEdit, SIGNAL(textEdited(const QString &)), this,
        SLOT(on_other_lineEdit_textEdited(const QString &)));
    connect(dbManager, SIGNAL(monthlyReportChanged(QStringList, bool)), this,
        SLOT(updateMonthlyReportUi(QStringList, bool)));
    connect(dbManager, SIGNAL(noDatabaseConnection(QSqlDatabase*)), this,
        SLOT(on_noDatabaseConnection(QSqlDatabase*)), Qt::DirectConnection);
    connect(dbManager, SIGNAL(noDatabaseConnection()), this,
        SLOT(on_noDatabaseConnection()));
    connect(dbManager, SIGNAL(reconnectedToDatabase()), this,
        SLOT(on_reconnectedToDatabase()));

    curClient = 0;
    curBook = 0;
    trans = 0;
//    currentshiftid = 1; // should change this. Set to 1 for testing;

    MainWindow::setupReportsScreen();

//    //display logged in user and current shift in status bar
//    QLabel *lbl_curUser = new QLabel("Logged in as: " + userLoggedIn + "  ");
//    QLabel *lbl_curShift = new QLabel("Shift Number: ");
//    statusBar()->addPermanentWidget(lbl_curUser);
//    statusBar()->addPermanentWidget(lbl_curShift);
//    lbl_curShift->setText("fda");

    dialog = new QProgressDialog();
    dialog->close();

    // Connect signals and slots for futureWatcher.
    connect(&futureWatcher, SIGNAL(finished()), dialog, SLOT(reset()));
    connect(dialog, SIGNAL(canceled()), &futureWatcher, SLOT(cancel()));
    connect(&futureWatcher, SIGNAL(progressRangeChanged(int,int)), dialog, SLOT(setRange(int,int)));
    connect(&futureWatcher, SIGNAL(progressValueChanged(int)), dialog, SLOT(setValue(int)));

    this->showMaximized();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*==============================================================================
DETECT WIDGET CHANGING SIGNAL
==============================================================================*/
//initialize widget when getinto that widget
void MainWindow::initCurrentWidget(int idx){
    switch(idx){
        case MAINMENU:  //WIDGET 0
            curClientID = "";
            registerType = NOREGISTER;
            ui->actionExport_to_PDF->setEnabled(false);
            break;
        case CLIENTLOOKUP:  //WIDGET 1
            initClientLookupInfo();
            if(caseWorkerUpdated){
                getCaseWorkerList();
                defaultRegisterOptions();
            }
            ui->tabWidget_cl_info->setCurrentIndex(0);
            if(registerType == EDITCLIENT)
                getClientInfo();
            registerType = NOREGISTER;
            ui->checkBox_search_anonymous->setChecked(false);
            ui->pushButton_search_client->setEnabled(true);
            //initimageview
            ui->actionExport_to_PDF->setEnabled(false);
            break;
        case BOOKINGLOOKUP: //WIDGET 2
            qDebug()<<"Client INFO";
            if(curClient != NULL){
                qDebug()<<"ID: " << curClientID << curClient->clientId;
                qDebug()<<"NAME: " << curClient->fullName;
                qDebug()<<"Balance: " << curClient->balance;
            }
            ui->startDateEdit->setDate(QDate::currentDate());
            ui->endDateEdit->setDate(QDate::currentDate().addDays(1));
            getProgramCodes();
            bookingSetup();
            clearTable(ui->bookingTable);
            editOverLap = false;

            break;
        case BOOKINGPAGE: //WIDGET 3
            //initcode

            break;
        case PAYMENTPAGE: //WIDGET 4
            popManagePayment();

            ui->editRemoveCheque->setHidden(true);

            break;
        case ADMINPAGE: //WIDGET 5
            //initcode
            break;
        case EDITUSERS: //WIDGET 6
            //initcode
            break;
        case EDITPROGRAM: //WIDGET 7
            //initcode
            break;
        case CASEFILE: //WIDGET 8
            ui->tabw_casefiles->setCurrentIndex(PERSIONACASEPLAN);
            ui->tableWidget_casefile_booking->verticalHeader()->show();
            ui->tableWidget_casefile_transaction->verticalHeader()->show();
            qDebug()<<"Client INFO";
            if(curClient != NULL){
                qDebug()<<"ID: " << curClientID << curClient->clientId;
                qDebug()<<"NAME: " << curClient->fullName;
                qDebug()<<"Balance: " << curClient->balance;
            }
            newTrans = true;
            newHistory = true;
            initCasefileTransactionTable();
            initPcp();
            ui->actionExport_to_PDF->setEnabled(true);
            break;
        case EDITBOOKING: //WIDGET 9
            ui->editLookupTable->clear();
            ui->editLookupTable->setRowCount(0);
            //initcode
            break;
        case CLIENTREGISTER:    //WIDGET 10
            clear_client_register_form();
            defaultRegisterOptions();           //combobox item add
            if(curClientID != NULL || curClientID != "")
                read_curClient_Information(curClientID);
            break;
        case REPORTS:    //WIDGET 11
            ui->dailyReport_tabWidget->setCurrentIndex(DEFAULTTAB);
            ui->shiftReport_tabWidget->setCurrentIndex(DEFAULTTAB);
            MainWindow::updateDailyReportTables(QDate::currentDate());
            MainWindow::getDailyReportStats(QDate::currentDate());
            MainWindow::updateShiftReportTables(QDate::currentDate(), currentshiftid);
            MainWindow::getShiftReportStats(QDate::currentDate(), currentshiftid);
            MainWindow::getCashFloat(QDate::currentDate(), currentshiftid);
            MainWindow::getMonthlyReport(QDate::currentDate().month(), QDate::currentDate().year());
            MainWindow::updateRestrictionTables();
            ui->actionExport_to_PDF->setEnabled(true);
            break;
        default:
            qDebug()<<"NO information about stackWidget idx : "<<idx;

    }
}

void MainWindow::on_bookButton_clicked()
{
    workFlow = BOOKINGPAGE;
    ui->stackedWidget->setCurrentIndex(CLIENTLOOKUP);
    addHistory(MAINMENU);
    qDebug() << "pushed page " << MAINMENU;
    /*ui->startDateEdit->setDate(QDate::currentDate());
    ui->endDateEdit->setDate(QDate::currentDate().addDays(1));
    if(firstTime){
        firstTime = false;
        getProgramCodes();
        bookingSetup();

    }
    ui->makeBookingButton->hide();
    ui->monthCheck->setChecked(false);
    */
}

void MainWindow::on_clientButton_clicked()
{
     workFlow = CLIENTLOOKUP;
     ui->stackedWidget->setCurrentIndex(CLIENTLOOKUP);
     addHistory(MAINMENU);
     qDebug() << "pushed page " << MAINMENU;
}

void MainWindow::on_paymentButton_clicked()
{
     workFlow = PAYMENTPAGE;
     ui->stackedWidget->setCurrentIndex(PAYMENTPAGE);
     addHistory(MAINMENU);
     qDebug() << "pushed page " << MAINMENU;
}

void MainWindow::on_editbookButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(EDITBOOKING);
    addHistory(MAINMENU);
    qDebug() << "pushed page " << MAINMENU;
}

void MainWindow::on_caseButton_clicked()
{
    workFlow = CASEFILE;
    ui->stackedWidget->setCurrentIndex(CLIENTLOOKUP);
    addHistory(MAINMENU);
    qDebug() << "pushed page " << MAINMENU;

}

void MainWindow::on_adminButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(ADMINPAGE);
    addHistory(MAINMENU);
    qDebug() << "pushed page " << MAINMENU;

}

/*==============================================================================
DEV TESTING BUTTONS (START)
==============================================================================*/
void MainWindow::on_actionDB_Connection_triggered()
{
}

void MainWindow::on_actionTest_Query_triggered()
{
    dbManager->printAll(dbManager->selectAll("Client"));
}

void MainWindow::on_actionFile_Upload_triggered()
{
    QString strFilePath = MainWindow::browse();
    if (!strFilePath.isEmpty())
    {
        QtConcurrent::run(dbManager, &DatabaseManager::uploadThread, strFilePath);
    }
    else
    {
        qDebug() << "Empty file path";
    }
}

void MainWindow::on_actionDownload_Latest_Upload_triggered()
{
    QtConcurrent::run(dbManager, &DatabaseManager::downloadThread);
}

void MainWindow::on_actionPrint_Db_Connections_triggered()
{
    dbManager->printDbConnections();
}

void MainWindow::on_actionUpload_Display_Picture_triggered()
{
    QString strFilePath = MainWindow::browse();
    if (!strFilePath.isEmpty())
    {
        QtConcurrent::run(dbManager, &DatabaseManager::uploadProfilePicThread, strFilePath);
    }
    else
    {
        qDebug() << "Empty file path";
    }
}

void MainWindow::on_actionDownload_Profile_Picture_triggered()
{
    QImage* img = new QImage();
    img->scaledToWidth(300);
    dbManager->downloadProfilePic(img);

    MainWindow::addPic(*img);
}
/*==============================================================================
DEV TESTING BUTTONS (END)
==============================================================================*/
/*==============================================================================
DEV TESTING AUXILIARY FUNCTIONS (START)
==============================================================================*/
QString MainWindow::browse()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::DirectoryOnly);
    QString strFilePath = dialog.getOpenFileName(this, tr("SelectFile"), "", tr("All Files (*.*)"));

    return strFilePath;
}
/*==============================================================================
DEV TESTING AUXILIARY FUNCTIONS (END)
==============================================================================*/

//COLIN STUFF /////////////////////////////////////////////////////////////////

void MainWindow::on_lunchCheck_clicked()
{
   QDate testDate = QDate::currentDate();
   testDate = testDate.addDays(32);
//   QDate otherDate = testDate.addDays(35);
  //curClient = new Client();
   //curClient->clientId = "1";

   MyCalendar* mc = new MyCalendar(this, curBook->startDate,curBook->endDate, curClient,1, curBook->room);
   mc->exec();
   delete(mc);
}

void MainWindow::on_paymentButton_2_clicked()
{
    trans = new transaction();
    double owed;
    //owed = curBook->cost;
    owed = ui->costInput->text().toDouble();
    QString note = "Booking: " + curBook->stringStart + " to " + curBook->stringEnd + " Cost: " + QString::number(curBook->cost, 'f', 2);
    payment * pay = new payment(this, trans, curClient->balance, owed , curClient, note, true, userLoggedIn, QString::number(currentshiftid));
    pay->exec();
    ui->stayLabel->setText(QString::number(curClient->balance, 'f', 2));
    qDebug() << "Done";
    delete(pay);


}
void MainWindow::on_startDateEdit_dateChanged()
{

    if(ui->startDateEdit->date() < QDate::currentDate()){
        ui->startDateEdit->setDate(QDate::currentDate());
    }
    if(ui->startDateEdit->date() > ui->endDateEdit->date()){
        QDate newD = ui->startDateEdit->date().addDays(1);
        ui->endDateEdit->setDate(newD);
    }
    clearTable(ui->bookingTable);
    ui->makeBookingButton->hide();
}

void MainWindow::on_wakeupCheck_clicked()
{
    MyCalendar* mc = new MyCalendar(this, curBook->startDate,curBook->endDate, curClient,2, curBook->room);
    mc->exec();
    delete(mc);
}

void MainWindow::on_endDateEdit_dateChanged()
{
    if(editOverLap){
        editOverLap = false;
    }
    else{
        editOverLap = false;
        ui->monthCheck->setChecked(false);
    }
    if(ui->endDateEdit->date() < ui->startDateEdit->date()){
        QDate newDate = ui->startDateEdit->date().addDays(1);
        ui->endDateEdit->setDate(newDate);

    }
    clearTable(ui->bookingTable);
    ui->makeBookingButton->hide();
}

void MainWindow::on_monthCheck_clicked(bool checked)
{
    clearTable(ui->bookingTable);
    ui->makeBookingButton->hide();
    if(checked)
    {
        editOverLap = true;
        QDate month = ui->startDateEdit->date();
        //month = month.addMonths(1);
        int days = month.daysInMonth();
        days = days - month.day();
        month = month.addDays(days + 1);
        ui->endDateEdit->setDate(month);
        ui->monthCheck->setChecked(true);
    }
    else{
        ui->monthCheck->setChecked(false);
        editOverLap = true;
    }
}
void MainWindow::bookingSetup(){

    ui->bookingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->bookingTable->verticalHeader()->hide();
   // ui->bookingTable->horizontalHeader()->setStretchLastSection(true);
    ui->bookingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->bookingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->bookingTable->setHorizontalHeaderLabels(QStringList() << "Room #" << "Location" << "Program" << "Description" << "Daily Cost" << "Monthly Cost");

}
void MainWindow::clearTable(QTableWidget * table){
    table->clearContents();
    table->setRowCount(0);
}

void MainWindow::on_editButton_clicked()
{
    setup = true;
    int row = ui->editLookupTable->selectionModel()->currentIndex().row();
    if(row == - 1){
        return;
    }
    ui->editDate->setEnabled(true);
    ui->editRoom->setEnabled(true);
    curBook = new Booking();
    popBookFromRow();
    popClientFromId(curBook->clientId);
    ui->stackedWidget->setCurrentIndex(EDITPAGE);
    ui->editUpdate->setEnabled(false);
    popEditPage();
    setBookSummary();
    ui->editUpdate->setEnabled(false);
    setup = false;
}
void MainWindow::popClientFromId(QString id){
    QSqlQuery result;
    curClient = new Client();

    result = dbManager->pullClient(id);
    result.seek(0, false);
    curClient->clientId = id;
    curClient->fName = result.record().value("FirstName").toString();
    curClient->mName = result.record().value("MiddleName").toString();
    curClient->lName = result.record().value("LastName").toString();
    curClient->fullName = curClient->fName + " " +  curClient->mName + " "
            + curClient->lName;
    QString balanceString = result.record().value("Balance").toString();
    balanceString.replace("$", "");
    curClient->balance =  balanceString.toDouble();
    // curClient->balance = result.record().value("Balance").toString().toDouble();


}

void MainWindow::popEditPage(){

    QSqlQuery result;
    result = dbManager->getPrograms();
    QString curProgram;
    QStringList programs;
    QString compProgram;
    int index = 0;
    int x = 0;
    ui->editOC->setText(QString::number(curBook->cost,'f',2));
    curProgram = curBook->program;
    while(result.next()){
        QString compProgram = result.value("ProgramCode").toString();
        if(curProgram == compProgram)
            index =x;
        programs << compProgram;
        x++;
    }
    ui->editProgramDrop->clear();
    ui->editProgramDrop->addItems(programs);
    ui->editProgramDrop->setCurrentIndex(index);
    ui->editRoomLabel->setText(curBook->room);
    ui->editDate->setDate(curBook->endDate);
    ui->editCost->setText(QString::number(curBook->cost));
    ui->editRoom->setEnabled(true);

}

void MainWindow::popBookFromRow(){
    int row = ui->editLookupTable->selectionModel()->currentIndex().row();
    if(row == - 1){
        return;
    }
    curBook->cost = ui->editLookupTable->item(row,5)->text().toDouble();
    curBook->stringStart = ui->editLookupTable->item(row, 2)->text();
    curBook->startDate = QDate::fromString(curBook->stringStart, "yyyy-MM-dd");
    curBook->stringEnd = ui->editLookupTable->item(row, 3)->text();
    curBook->endDate = QDate::fromString(curBook->stringEnd, "yyyy-MM-dd");
    curBook->stayLength = curBook->endDate.toJulianDay() - curBook->startDate.toJulianDay();
    if(ui->editLookupTable->item(row,6)->text() == "YES"){
      curBook->monthly = true;
         }
     else{
         curBook->monthly = false;
     }
    curBook->clientId = ui->editLookupTable->item(row, 8)->text();

    curBook->program = ui->editLookupTable->item(row,4)->text();
    curBook->room = ui->editLookupTable->item(row,1)->text();
    curBook->bookID = ui->editLookupTable->item(row, 7)->text();
    curBook->roomId = ui->editLookupTable->item(row, 9)->text();

}
void MainWindow::popManagePayment(){
    QStringList dropItems;
    ui->cbox_payDateRange->clear();
    ui->mpTable->clear();
    ui->mpTable->setRowCount(0);
    ui->btn_payDelete->setText("Delete");

    dropItems << "" << "Today" << "Last 3 Days" << "This Month"
              <<  QDate::longMonthName(QDate::currentDate().month() - 1)
              << QDate::longMonthName(QDate::currentDate().month() - 2)
              << "ALL";
    ui->cbox_payDateRange->addItems(dropItems);
}

void MainWindow::on_cbox_payDateRange_activated(int index)
{
    ui->editRemoveCheque->setHidden(true);

    QString startDate;
    QDate endDate = QDate::currentDate();
    QDate hold = QDate::currentDate();
    ui->btn_payDelete->setText("Delete");
    int days, move;
    switch(index){
    case 0:
        return;
        break;
    case 1:
        startDate = QDate::currentDate().toString(Qt::ISODate);
        break;
    case 2:
        hold = hold.addDays(-3);
        startDate = hold.toString(Qt::ISODate);
        break;
    case 3:
        days = hold.daysInMonth();
        days = days - hold.day();
        endDate = hold.addDays(days);
        move = hold.day() -1;
        hold = hold.addDays(move * -1);
        break;
    case 4:
        hold = hold.addMonths(-1);
        days = hold.daysInMonth();

        move = hold.day() -1;
        days = days - hold.day();
        endDate = hold.addDays(days);
        hold = hold.addDays(move * -1);
        break;
    case 5:
        hold = hold.addMonths(-2);
        days = hold.daysInMonth();

        move = hold.day() -1;
        days = days - hold.day();
        endDate = hold.addDays(days);
        hold = hold.addDays(move * -1);
        break;
    case 6:
        hold = QDate::fromString("1970-01-01", "yyyy-MM-dd");
        endDate = QDate::fromString("2222-01-01", "yyyy-MM-dd");
        break;

    }
    QStringList heads;
    QStringList cols;
    QSqlQuery tempSql = dbManager->getTransactions(hold, endDate);
    heads << "Date"  <<"First" << "Last" << "Amount" << "Type" << "Method" << "Notes"  << "" << "";
    cols << "Date" <<"FirstName"<< "LastName"  << "Amount" << "TransType" << "Type" << "Notes" << "TransacId" << "ClientId";
    populateATable(ui->mpTable, heads, cols, tempSql, false);
    ui->mpTable->setColumnHidden(7, true);
    ui->mpTable->setColumnHidden(8, true);

}


void MainWindow::on_btn_payListAllUsers_clicked()
{
    ui->editRemoveCheque->setHidden(true);

    ui->btn_payDelete->setText("Add Payment");
    QStringList cols;
    QStringList heads;
    QSqlQuery tempSql = dbManager->getOwingClients();
    heads << "First" << "Last" << "DOB" << "Balance" << "";
    cols << "FirstName" << "LastName" << "Dob" << "Balance" << "ClientId";
    ui->mpTable->setColumnHidden(4, true);
    populateATable(ui->mpTable, heads, cols, tempSql, false);

}

void MainWindow::on_editSearch_clicked()
{
    /*ui->editLookupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->editLookupTable->verticalHeader()->hide();
    ui->editLookupTable->horizontalHeader()->setStretchLastSection(true);
    ui->editLookupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->editLookupTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->editLookupTable->setRowCount(0);
    ui->editLookupTable->clear();
    ui->editLookupTable->setHorizontalHeaderLabels(QStringList()
                                                << "Created" << "Start" << "End" << "Monthly" << "Room" << "Client" << "Program" << "Cost"
                                                 << "Lunch" << "Wakeup" << "" << "");
    ui->editLookupTable->setColumnHidden(10, true);
    ui->editLookupTable->setColumnHidden(11, true);
*/
    QSqlQuery result;
    QString user = "";
    if(ui->editClient->text() != ""){
        user = ui->editClient->text();
    }

    result = dbManager->getActiveBooking(user, true);
//    int numCols = result.record().count();

    QStringList headers;
    QStringList cols;
    headers << "Client" << "Room" << "Start" << "End" << "Program" << "Cost" << "Monthly" << "" << "" << "";
    cols << "ClientName" << "SpaceCode" << "StartDate" << "EndDate" << "ProgramCode" << "Cost" << "Monthly" << "BookingId" << "ClientId" << "SpaceId";
    populateATable(ui->editLookupTable, headers, cols, result, false);
    ui->editLookupTable->hideColumn(7);
    ui->editLookupTable->hideColumn(8);
    ui->editLookupTable->hideColumn(9);
    //dbManager->printAll(result);




    /*while (result.next()) {
        ui->editLookupTable->insertRow(x);


        QStringList row;
        row << result.value(1).toString() << result.value(7).toString() << result.value(8).toString() << result.value(12).toString()
            << result.value(3).toString() << result.value(13).toString() << result.value(5).toString() << result.value(6).toString()
                  << result.value(9).toString() << result.value(10).toString() << result.value(4).toString() << result.value(0).toString();
        for (int i = 0; i < 12; ++i)
        {
            ui->editLookupTable->setItem(x,i, new QTableWidgetItem(row.at(i)));


        }
        x++;


    }*/
}
void MainWindow::on_bookingSearchButton_clicked()
{
    if(!book.checkValidDate(ui->startDateEdit->date(), ui->endDateEdit->date())){
        //Pop up error or something
        return;
    }
    QString program = ui->programDropdown->currentText();

    QSqlQuery result = dbManager->getCurrentBooking(ui->startDateEdit->date(), ui->endDateEdit->date(), program);

   /* ui->bookingTable->setRowCount(0);
    ui->bookingTable->clear();
    ui->bookingTable->setHorizontalHeaderLabels(QStringList() << "Room #" << "Location" << "Program" << "Description" << "Cost" << "Monthly");
    int numCols = result.record().count();

    int x = 0;
    while (result.next()) {
        ui->bookingTable->insertRow(x);
        for (int i = 0; i < numCols; ++i)
        {
            if(i == 4){
                ui->bookingTable->setItem(x,i, new QTableWidgetItem(QString::number(result.value(i).toString().toDouble(), 'f', 2)));
                continue;
            }
            ui->bookingTable->setItem(x,i, new QTableWidgetItem(result.value(i).toString()));


        }

        x++;

    }
*/
    QStringList headers, cols;
    headers << "Room#" << "Program" << " Type" << "Daily Cost" << "Monthly Cost" << "";
    cols << "SpaceCode" << "ProgramCodes" << "type" << "cost" << "Monthly" << "SpaceId";
    populateATable(ui->bookingTable, headers, cols, result,false);
    ui->bookingTable->setColumnHidden(5, true);
    ui->makeBookingButton->show();
}
//PARAMS - The table, list of headers, list of table column names, the sqlquery result, STRETCH - stretch mode true/false
void MainWindow::populateATable(QTableWidget * table, QStringList headers, QStringList items, QSqlQuery result, bool stretch){
    table->clear();
    table->setRowCount(0);

    if(headers.length() != items.length())
        return;

    if(stretch)
        table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    int colCount = headers.size();
    table->setColumnCount(colCount);
    if(headers.length() != 0){
        table->setHorizontalHeaderLabels(headers);
    }
    int x = 0;
    while(result.next()){
        table->insertRow(x);
        for(int i = 0; i < colCount; i++){
            table->setItem(x, i, new QTableWidgetItem(result.value(items.at(i)).toString()));


        }


        x++;
    }
}

void MainWindow::setBooking(int row){
    curBook->clientId = curClient->clientId;
    curBook->startDate = ui->startDateEdit->date();
    curBook->endDate = ui->endDateEdit->date();
    curBook->stringStart = ui->startDateEdit->date().toString(Qt::ISODate);
    curBook->stringEnd = ui->endDateEdit->date().toString(Qt::ISODate);
    curBook->monthly = ui->monthCheck->isChecked();
    curBook->program = ui->programDropdown->currentText();
    curBook->room = ui->bookingTable->item(row,0)->text();
    curBook->stayLength = ui->endDateEdit->date().toJulianDay() - ui->startDateEdit->date().toJulianDay();
    double potentialCost = 999999;
    double dailyCost = 0;
    curBook->roomId = ui->bookingTable->item(row, 5)->text();
    QString dayCost = QString::number(ui->bookingTable->item(row, 3)->text().toDouble(), 'f', 2);
    dailyCost = dayCost.toDouble();
    dailyCost = curBook->stayLength * dailyCost;
    if(ui->monthCheck->isChecked()){

        potentialCost = ui->bookingTable->item(row, 4)->text().toInt();
        if(dailyCost < potentialCost){
            curBook->cost = dailyCost;
        }
        else{
            curBook->cost = potentialCost;
        }
    }
    else{
        curBook->cost = dailyCost;
    }

}

void MainWindow::on_makeBookingButton_clicked()
{
    addHistory(BOOKINGLOOKUP);
    int row = ui->bookingTable->selectionModel()->currentIndex().row();
    if(row == - 1){
        return;
    }
    //curClient = new Client();
   //popClientFromId("1");
    ui->stackedWidget->setCurrentIndex(BOOKINGPAGE);
//    int rowNum = ui->bookingTable->columnCount();
    QStringList data;
    curBook = new Booking;
    setBooking(row);
    ui->stackedWidget->setCurrentIndex(BOOKINGPAGE);
    populateBooking();
    ui->makeBookingButton_2->setEnabled(true);

}
void MainWindow::populateBooking(){
    //room, location, program, description, cost, program, start, end, stayLength
    ui->startLabel->setText(curBook->stringStart);
    ui->endLabel->setText(curBook->stringEnd);
    ui->roomLabel->setText(curBook->room);
    ui->costInput->setText(QString::number(curBook->cost));
    ui->programLabel->setText(curBook->program);
    ui->lengthOfStayLabel->setText(QString::number(curBook->stayLength));
    // - curBook->cost + curBook->paidTotal, 'f', 2)
    ui->stayLabel->setText(QString::number(curClient->balance));
    if(curBook->monthly){
        ui->monthLabel->setText("YES");
    }
    else{
        ui->monthLabel->setText("NO");
    }
}

void MainWindow::getProgramCodes(){
    QSqlQuery result = dbManager->getPrograms();
//    int i = 0;
    ui->programDropdown->clear();
    while(result.next()){
        ui->programDropdown->addItem(result.value(0).toString());
    }
}
void MainWindow::setBookSummary(){
    ui->editStartDate->setText(curBook->stringStart);
    ui->editEndDate->setText(curBook->stringEnd);
    QDate end = QDate::fromString(curBook->stringEnd, "yyyy-MM-dd");
    int length = end.toJulianDay() - curBook->startDate.toJulianDay();
    curBook->endDate = end;
    curBook->stayLength = length;
    ui->editCostLabel->setText(QString::number(curBook->cost, 'f', 2));
    curBook->monthly == true ? ui->editMonthly->setText("Yes") : ui->editMonthly->setText("No");
    ui->editProgramLabel->setText(curBook->program);
    ui->editLengthOfStay->setText(QString::number(curBook->stayLength));
    ui->editRoomLabel_2->setText(curBook->room);
    ui->editCost->setText(QString::number(curBook->cost, 'f', 2));
    //ui->editRefundAmt->setText(QString::number(ui->editOC->text().toDouble() - curBook->cost));

}

void MainWindow::on_editUpdate_clicked()
{
    ui->editUpdate->setEnabled(false);
    double updateBalance;
    if(!checkNumber(ui->editCost->text()))
        return;

    ui->editRoom->setEnabled(true);
    ui->editDate->setEnabled(true);
    ui->editLunches->setEnabled(true);
    ui->editWakeup->setEnabled(true);
    if(ui->editRefundLabel->text() == "Refund"){
        curBook->monthly = false;
        updateBalance = curClient->balance + ui->editRefundAmt->text().toDouble();
    }
    else{
        updateBalance = curClient->balance - ui->editRefundAmt->text().toDouble();
    }
    curBook->cost = ui->editCost->text().toDouble();
    if(!dbManager->updateBalance(updateBalance, curClient->clientId)){
        qDebug() << "Error inserting new balance";
        return;
    }
    ui->editOC->setText(QString::number(curBook->cost, 'f', 2));
//    double qt = ui->editRefundAmt->text().toDouble();
    ui->editRefundAmt->setText("0.0");

    curClient->balance = updateBalance;
    curBook->endDate = ui->editDate->date();
    curBook->stringEnd = curBook->endDate.toString(Qt::ISODate);
    curBook->program = ui->editProgramDrop->currentText();
    updateBooking(*curBook);
    setBookSummary();
    dbManager->removeLunchesMulti(curBook->endDate, curClient->clientId);
    dbManager->deleteWakeupsMulti(curBook->endDate, curClient->clientId);
    curBook->stayLength = curBook->endDate.toJulianDay() - curBook->startDate.toJulianDay();
    dbManager->addHistoryFromId(curBook->bookID, userLoggedIn, QString::number(currentshiftid), "EDIT");
}
bool MainWindow::doMessageBox(QString message){
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm", message, QMessageBox::Yes | QMessageBox::No);
    if(reply == QMessageBox::Yes){
        return true;
    }
    return false;

}

double MainWindow::calcRefund(QDate old, QDate n){
    int updatedDays = n.toJulianDay() - old.toJulianDay();
    double cpd;
    double updatedCost;
    if(updatedDays > 0){
        //DO A POPUP ERROR HERE
        if(curBook->monthly){
            qDebug() << "NOT POSSIBLE -> is a monthly booking";
            ui->editDate->setDate(curBook->endDate);
            updatedDays = 0;
        }
        cpd = curBook->cost / (double)curBook->stayLength;
        //curBook->cost += updatedDays * cpd;
        return updatedDays * cpd;
    }
    else{
        //ERROR POPUP FOR INVALID DATe
        if(n < curBook->startDate){


            ui->editDate->setDate(curBook->startDate);
            updatedDays = curBook->stayLength * -1;

        }
        cpd = curBook->cost / (double)curBook->stayLength;
        updatedCost = cpd * (curBook->stayLength + updatedDays);

        if(curBook->monthly){
            double normalRate = dbManager->getRoomCost(curBook->roomId);
            updatedCost = normalRate * (curBook->stayLength + updatedDays);
            if(updatedCost > curBook->cost)
                return 0;

        }
        return (curBook->cost - updatedCost) * -1;
    }
}

bool MainWindow::checkNumber(QString num){
    int l = num.length();
    if(l > 8)
        return false;
    int period = 0;
    char copy[l];
    strcpy(copy, num.toStdString().c_str());
    for(int i = 0; i < num.length(); i++){
        if(copy[i] == '.'){
            if(period)
                return false;
            period++;
            continue;
        }

        if(!isdigit(copy[i]))
            return false;
    }
    return true;
}
bool MainWindow::updateBooking(Booking b){
    QString query;
    QString monthly;
    curBook->monthly == true ? monthly = "YES" : monthly = "NO";
    query = "UPDATE BOOKING SET " +
            QString("SpaceID = '") + b.roomId + "', " +
            QString("ProgramCode = '") + b.program + "', " +
            QString("Cost = '") + QString::number(b.cost) + + "', " +
            QString("EndDate = '") + b.stringEnd + "', " +
            QString("Monthly = '") + monthly + "'" +
            QString(" WHERE BookingId = '") +
            b.bookID + "'";
    return dbManager->updateBooking(query);
}
void MainWindow::on_btn_payDelete_clicked()
{
    if(ui->btn_payDelete->text() == "Cash Cheque")
    {
        int index = ui->mpTable->selectionModel()->currentIndex().row();
        if(index == -1)
            return;
        transaction * t = new transaction();
        t->chequeNo = "";
        AddMSD * amd = new AddMSD(this,t);
        amd->exec();
        if(t->chequeNo == "NO")
            return;
        updateCheque(index, t->chequeNo);
        delete(amd);
    }
    else if(ui->btn_payDelete->text() == "Delete"){
        int index = ui->mpTable->selectionModel()->currentIndex().row();
        if(index == -1)
            return;

        getTransactionFromRow(index);
    }
    else{
        int index = ui->mpTable->selectionModel()->currentIndex().row();
        if(index == -1)
            return;
        handleNewPayment(index);
    }

}

void MainWindow::handleNewPayment(int row){
    curClient = new Client();
    trans = new transaction();
    curClient->clientId = ui->mpTable->item(row,4)->text();
    QString balanceString = ui->mpTable->item(row, 3)->text();
    balanceString.replace("$", "");
    double balance = balanceString.toDouble();
    // double balance = ui->mpTable->item(row, 3)->text().toDouble();
    curClient->balance = balance;
    QString note = "Paying Outstanding Balance";

    payment * pay = new payment(this, trans, curClient->balance, 0 , curClient, note, true, userLoggedIn, QString::number(currentshiftid));
    pay->exec();
    ui->mpTable->removeRow(row);
    delete(pay);
}

void MainWindow::updateCheque(int row, QString chequeNo){
    QString transId = ui->mpTable->item(row, 6)->text();
    double retAmt = ui->mpTable->item(row, 3)->text().toDouble();
    QString clientId = ui->mpTable->item(row, 5)->text();
    curClient = new Client();
    popClientFromId(clientId);
    double curBal = curClient->balance + retAmt;
    if(dbManager->setPaid(transId, chequeNo )){
        if(!dbManager->updateBalance(curBal, clientId)){
                qDebug() << "BIG ERROR - removed transacton but not update balance";
                return;
        }
    }
    ui->mpTable->removeRow(row);
}

void MainWindow::getTransactionFromRow(int row){
    QString transId = ui->mpTable->item(row, 7)->text();

    QString type = ui->mpTable->item(row, 4)->text();
    double retAmt = ui->mpTable->item(row, 3)->text().toDouble();
    QString clientId = ui->mpTable->item(row, 8)->text();
    curClient = new Client();
    popClientFromId(clientId);
    double curBal = curClient->balance;

    if(type == "Payment"){
        curBal -= retAmt;
    }
    else if(type == "Refund"){
        curBal += retAmt;
    }
    else{
        //error - not a payment or refund
        return;
    }
    dbManager->updateBalance(curBal, clientId);
    dbManager->removeTransaction(transId);
    ui->mpTable->removeRow(row);

}

void MainWindow::on_btn_payOutstanding_clicked()
{
    ui->btn_payDelete->setText("Cash Cheque");
    ui->editRemoveCheque->setHidden(false);
    QSqlQuery result;
    result = dbManager->getOutstanding();
    QStringList headers;
    QStringList cols;
    headers << "Date" << "First" << "Last" << "Amount" << "Notes" << "" << "";
    cols << "Date" << "FirstName" << "LastName" << "Amount" << "Notes" << "ClientId" << "TransacId";
    populateATable(ui->mpTable, headers, cols, result, false);
    ui->mpTable->setColumnHidden(6, true);
    ui->mpTable->setColumnHidden(5, true);
}


void MainWindow::on_editDate_dateChanged(const QDate &date)
{
    if(!setup){
        ui->editLunches->setEnabled(false);
        ui->editWakeup->setEnabled(false);
        ui->editUpdate->setEnabled(true);

    }
    ui->editRoom->setEnabled(false);
    QDate nextStart = date;

    if(date > curBook->endDate){
        QSqlQuery result;
        result = dbManager->getNextBooking(curBook->endDate, curBook->roomId);
        int x = 0 ;
        while(result.next()){
            if(x == 0){
                nextStart = QDate::fromString(result.value("StartDate").toString(), "yyyy-MM-dd");
            }
            x++;
        }
        if(!x){
            nextStart = date;
        }
        ui->editDate->setDate(nextStart);
    }
    if(date < curBook->startDate){
        ui->editDate->setDate(curBook->startDate);
    }

    qDebug() << "Edit date called";
    double refund = 0;
    double newCost;
    QString cost = ui->editCost->text();
    if(!checkNumber(cost)){
        qDebug() << "NON NUMBER";
        return;
    }
    //curBook->cost = QString::number(curBook->cost, 'f', 2).toDouble();

    refund = calcRefund(curBook->endDate, nextStart);
    qDebug() << "REFUNDING" << refund;


    newCost = curBook->cost + refund;
    ui->editCost->setText(QString::number(newCost));



}

void MainWindow::on_editManagePayment_clicked()
{
    trans = new transaction();
    double owed;

    owed = ui->editRefundAmt->text().toDouble();
    bool type;
    ui->editRefundLabel->text() == "Refund" ? type = false : type = true;
    if(!type){
        owed *= -1;
    }
    QString note = "";
    payment * pay = new payment(this, trans, curClient->balance, owed , curClient, note, type, userLoggedIn, QString::number(currentshiftid));
    pay->exec();
    delete(pay);
}

void MainWindow::on_editCost_textChanged()
{
    ui->editUpdate->setEnabled(true);
    double newCost = ui->editCost->text().toDouble();
    double refund = ui->editCancel->text().toDouble();
    double origCost = ui->editOC->text().toDouble();

    if(newCost < origCost){
        ui->editRefundLabel->setText("Refund");
        double realRefund = newCost - origCost + refund;

        if(realRefund > 0)
            realRefund = 0;
        realRefund = realRefund * -1;
        ui->editRefundAmt->setText(QString::number(realRefund, 'f', 2));
    }
    else{
        ui->editRefundLabel->setText("Owed");
        ui->editRefundAmt->setText(QString::number(newCost - origCost, 'f', 2));
    }
}

void MainWindow::on_editCancel_textChanged()
{
    double newCost = ui->editCost->text().toDouble();
    double refund = ui->editCancel->text().toDouble();
    double origCost = ui->editOC->text().toDouble();
    if(newCost < origCost){
        ui->editRefundLabel->setText("Refund");
        double realRefund = newCost - origCost + refund;
        if(realRefund > 0)
            realRefund = 0;
        realRefund = realRefund * -1;
        ui->editRefundAmt->setText(QString::number(realRefund, 'f', 2));
    }
    else{
        ui->editRefundLabel->setText("Owed");
        ui->editRefundAmt->setText(QString::number(newCost - origCost, 'f', 2));
    }
}

void MainWindow::on_editRoom_clicked()
{
   // swapper * swap = new Swapper();
    EditRooms * edit = new EditRooms(this, curBook, userLoggedIn, QString::number(currentshiftid), curClient);
    edit->exec();
    setBookSummary();
    ui->editOC->setText(QString::number(curBook->cost,'f',2));
    ui->editRefundAmt->setText("0");
    ui->editUpdate->setEnabled(false);
    ui->editRoomLabel->setText(curBook->room);
    delete(edit);

}

void MainWindow::on_pushButton_bookRoom_clicked()
{
    addHistory(CLIENTLOOKUP);
    curClient = new Client();
    int nRow = ui->tableWidget_search_client->currentRow();
    if (nRow <0){
        if(curClientID == NULL)
            return;
        else{
            curClient->clientId = curClientID;
            curClient->fName = ui->label_cl_info_fName_val->text();
            curClient->mName = ui->label_cl_info_mName_val->text();
            curClient->lName =  ui->label_cl_info_lName_val->text();
            QString balanceString = ui->label_cl_info_balance_amt->text();
            balanceString.replace("$", "");
            curClient->balance =  balanceString.toFloat();
            curClient->fullName = QString(curClient->lName + ", " + curClient->fName + " " + curClient->mName);
            // curClient->fullName = QString(curClient->fName + " " + curClient->mName + " " + curClient->lName);
        }
    }
    else{
        curClientID = curClient->clientId = ui->tableWidget_search_client->item(nRow, 0)->text();
        curClient->fName =  ui->tableWidget_search_client->item(nRow, 1)->text();
        curClient->mName =  ui->tableWidget_search_client->item(nRow, 2)->text();
        curClient->lName =  ui->tableWidget_search_client->item(nRow, 3)->text();
        QString balanceString = ui->tableWidget_search_client->item(nRow, 5)->text();
        balanceString.replace("$", "");
        curClient->balance =  balanceString.toFloat();
        curClient->fullName = QString(curClient->lName + ", " + curClient->fName + " " + curClient->mName);
        // curClient->fullName = QString(curClient->fName + " " + curClient->mName + " " + curClient->lName);
    }



    // qDebug()<<"ID: " << curClientID << curClient->clientId;
    // qDebug()<<"NAME: " << curClient->fullName;
    // qDebug()<<"Balance: " << curClient->balance;

    ui->stackedWidget->setCurrentIndex(BOOKINGLOOKUP);

}

void MainWindow::on_makeBookingButton_2_clicked()
{

    if(!checkNumber(ui->costInput->text()))
        return;

    backStack.clear();
    if(!doMessageBox("Finalize booking and add to database?")){
        return;
    }
    ui->actionBack->setEnabled(false);
    ui->makeBookingButton_2->setEnabled(false);

    curBook->lunch = "NULL";
    curBook->wakeTime = "NULL";
    QString month;
    if(curBook->monthly){
        month = "YES";
    }
    else{
        month = "NO";
    }
    double cost = QString::number(ui->costInput->text().toDouble(), 'f', 2).toDouble();
    QDate today = QDate::currentDate();
    QString values;
    QString todayDate = today.toString(Qt::ISODate);
    values = "'" + today.toString(Qt::ISODate) + "','" + curBook->stringStart + "','" + curBook->roomId + "','" +
             curClient->clientId + "','" + curBook->program + "','" + QString::number(cost) + "','" + curBook->stringStart
             + "','" + curBook->stringEnd + "','" + "YES'" + ",'" + month + "','" + curClient->fullName +"'";
//    QDate next = curBook->startDate;
    //QDate::fromString(ui->startLabel->text(), "yyyy-MM-dd");
    curBook->cost = cost;
   // insertIntoBookingHistory(QString clientName, QString spaceId, QString program, QDate start, QDate end, QString action, QString emp, QString shift){
    qDebug()<<"check booking"<<curBook->roomId;
    if(!dbManager->insertBookingTable(values)){
        qDebug() << "ERROR INSERTING BOOKING";
    }
    if(!dbManager->updateBalance(curClient->balance - curBook->cost, curClient->clientId)){
        qDebug() << "ERROR ADDING TO BALANCE UPDATE";
    }
    if(!dbManager->insertIntoBookingHistory(curClient->fullName, curBook->room, curBook->program, curBook->stringStart, curBook->stringEnd, "NEW", userLoggedIn, QString::number(currentshiftid), curClient->clientId)){
        qDebug() << "ERROR INSERTING INTO BOOKING HISTORY";
    }

    ui->stackedWidget->setCurrentIndex(CONFIRMBOOKING);
    populateConfirm();

 }

void MainWindow::populateConfirm(){
    ui->confirmCost->setText(QString::number(curBook->cost, 'f', 2));
    ui->confirmEnd->setText(curBook->stringEnd);
    ui->confirmStart->setText(curBook->stringStart);
    ui->confirmLength->setText(QString::number(curBook->stayLength));
    if(curBook->monthly){
        ui->confirmMonthly->setText("YES");
    }else{
        ui->confirmMonthly->setText("NO");
    }
    ui->confirmRoom->setText(curBook->room);
    ui->confirmPaid->setText(QString::number(curClient->balance));
    ui->confirmProgram->setText(curBook->program);


}

//void MainWindow::on_monthCheck_stateChanged(int arg1)
//{

//}


//END COLIN ////////////////////////////////////////////////////////////////////



void MainWindow::on_EditUserButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(EDITUSERS);
    addHistory(ADMINPAGE);
    qDebug() << "pushed page " << ADMINPAGE;

}

void MainWindow::on_EditProgramButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(EDITPROGRAM);
    addHistory(ADMINPAGE);
    qDebug() << "pushed page " << ADMINPAGE;

}

void MainWindow::on_actionMain_Menu_triggered()
{
    addHistory(ui->stackedWidget->currentIndex());
    ui->stackedWidget->setCurrentIndex(MAINMENU);

    //reset client lookup buttons
    ui->pushButton_bookRoom->setEnabled(false);
    ui->pushButton_processPaymeent->setEnabled(false);
    ui->pushButton_editClientInfo->setEnabled(false);
    ui->pushButton_CaseFiles->setEnabled(false);
}


void MainWindow::on_pushButton_RegisterClient_clicked()
{
    addHistory(CLIENTLOOKUP);
    registerType = NEWCLIENT;

    curClientID = "";
    ui->stackedWidget->setCurrentIndex(CLIENTREGISTER);
    ui->label_cl_infoedit_title->setText("Register Client");
    ui->button_register_client->setText("Register");
    ui->dateEdit_cl_rulesign->setDate(QDate::currentDate());

    defaultRegisterOptions();
}

void MainWindow::on_pushButton_editClientInfo_clicked()
{
    addHistory(CLIENTLOOKUP);
    registerType = EDITCLIENT;

    getCurrentClientId();

    ui->stackedWidget->setCurrentIndex(CLIENTREGISTER);
    ui->label_cl_infoedit_title->setText("Edit Client Information");
    ui->button_register_client->setText("Edit");
    //getCurrentClientId();
}


void MainWindow::on_reportsButton_clicked()
{
    if (dbManager->checkDatabaseConnection())
    {
        ui->stackedWidget->setCurrentIndex(REPORTS);
        addHistory(MAINMENU);
        qDebug() << "pushed page " << MAINMENU;
    }
}


/*===================================================================
  REGISTRATION PAGE
  ===================================================================*/

void MainWindow::getCurrentClientId(){
    int nRow = ui->tableWidget_search_client->currentRow();
    if (nRow <0)
        return;
    curClientID = ui->tableWidget_search_client->item(nRow, 0)->text();

}

//Client Regiter widget [TAKE A PICTURE] button
void MainWindow::on_button_cl_takePic_clicked()
{
    TakePhoto *camDialog = new TakePhoto();

    connect(camDialog, SIGNAL(showPic(QImage)), this, SLOT(addPic(QImage)));
    camDialog->show();
}

/*------------------------------------------------------------------
  add picture into graphicview (after taking picture in pic dialog
  ------------------------------------------------------------------*/
void MainWindow::addPic(QImage pict){

  //  qDebug()<<"ADDPIC";
    profilePic = pict.copy();
    QPixmap item = QPixmap::fromImage(pict);
    QPixmap scaled = QPixmap(item.scaledToWidth((int)(ui->graphicsView_cl_pic->width()*0.9), Qt::SmoothTransformation));
    QGraphicsScene *scene = new QGraphicsScene();
    scene->addPixmap(QPixmap(scaled));
    ui->graphicsView_cl_pic->setScene(scene);
    ui->graphicsView_cl_pic->show();
}

void MainWindow::on_button_cl_delPic_clicked()
{
    QGraphicsScene *scene = new QGraphicsScene();
    scene->clear();
    profilePic = (QImage)NULL;
    ui->graphicsView_cl_pic->setScene(scene);

    //delete picture function to database

}

void MainWindow::on_button_clear_client_regForm_clicked()
{
    clear_client_register_form();
}

void MainWindow::getListRegisterFields(QStringList* fieldList)
{
    QString caseWorkerId = QString::number(caseWorkerList.value(ui->comboBox_cl_caseWorker->currentText()));
    if(caseWorkerId == "0")
        caseWorkerId = "";

    QString firstName = ui->lineEdit_cl_fName->text();
    QString middleName = ui->lineEdit_cl_mName->text();
    QString lastName = ui->lineEdit_cl_lName->text();

    if (!firstName.isEmpty())
        firstName[0].toUpper();
    if (!middleName.isEmpty())
        middleName[0].toUpper();
    if (!lastName.isEmpty())
        lastName[0].toUpper();

    *fieldList << firstName
               << middleName
               << lastName
               << ui->dateEdit_cl_dob->date().toString("yyyy-MM-dd")
               << ui->lineEdit_cl_SIN->text()
               << ui->lineEdit_cl_GANum->text()
               << caseWorkerId   //QString::number(caseWorkerList.value(ui->comboBox_cl_caseWorker->currentText())) //grab value from case worker dropdown I don't know how to do it
               << ui->dateEdit_cl_rulesign->date().toString("yyyy-MM-dd")
               << ui->lineEdit_cl_nok_name->text()
               << ui->lineEdit_cl_nok_relationship->text()
               << ui->lineEdit_cl_nok_loc->text()
               << ui->lineEdit_cl_nok_ContactNo->text()
               << ui->lineEdit_cl_phys_name->text()
               << ui->lineEdit_cl_phys_ContactNo->text()
               << ui->lineEdit_cl_supporter_Name->text()
               << ui->lineEdit_cl_supporter_ContactNo->text()
               << ui->lineEdit_cl_supporter2_Name->text()
               << ui->lineEdit_cl_supporter2_ContactNo->text()
               << ui->comboBox_cl_status->currentText() //grab value from status dropdown
               << ui->plainTextEdit_cl_comments->toPlainText();

}

void MainWindow::getRegisterLogFields(QStringList* fieldList)
{
    QString fullName = ui->lineEdit_cl_fName->text() + " " + ui->lineEdit_cl_lName->text();
    QString action;
    if(registerType == NEWCLIENT || ui->button_register_client->text() == "Register")
        action = "Registered";
    else
        action = "Updated";

    *fieldList << fullName
               << action
               << QDate::currentDate().toString("yyyy-MM-dd")
               << QString::number(currentshiftid) // get shift number
               << QTime::currentTime().toString("hh:mm:ss")
               << userLoggedIn; //employee name

}

void MainWindow::clear_client_register_form(){
    ui->lineEdit_cl_fName->clear();
    ui->lineEdit_cl_mName->clear();
    ui->lineEdit_cl_lName->clear();
    ui->lineEdit_cl_SIN->clear();
    ui->lineEdit_cl_GANum->clear();
    ui->comboBox_cl_caseWorker->setCurrentIndex(0);
    ui->lineEdit_cl_nok_name->clear();
    ui->lineEdit_cl_nok_relationship->clear();
    ui->lineEdit_cl_nok_loc->clear();
    ui->lineEdit_cl_nok_ContactNo->clear();
    ui->lineEdit_cl_phys_name->clear();
    ui->lineEdit_cl_phys_ContactNo->clear();
    ui->lineEdit_cl_supporter_Name->clear();
    ui->lineEdit_cl_supporter_ContactNo->clear();
    ui->lineEdit_cl_supporter2_Name->clear();
    ui->lineEdit_cl_supporter2_ContactNo->clear();

    ui->comboBox_cl_status->setCurrentIndex(0);
    ui->plainTextEdit_cl_comments->clear();
    QDate defaultDob= QDate::fromString("1990-01-01","yyyy-MM-dd");
    ui->dateEdit_cl_dob->setDate(defaultDob);
    ui->dateEdit_cl_rulesign->setDate(QDate::currentDate());

    QPalette pal;
    pal.setColor(QPalette::Normal, QPalette::WindowText, Qt::black);
    ui->label_cl_fName->setPalette(pal);
    ui->label_cl_mName->setPalette(pal);
    ui->label_cl_lName->setPalette(pal);


    on_button_cl_delPic_clicked();
}

//read client information to edit
void MainWindow::read_curClient_Information(QString ClientId){
//    QString searchClientQ = "SELECT * FROM Client WHERE ClientId = "+ ClientId;
//    qDebug()<<"SEARCH QUERY: " + searchClientQ;
   // QSqlQuery clientInfo = dbManager->execQuery("SELECT * FROM Client WHERE ClientId = "+ ClientId);
    QSqlQuery clientInfo = dbManager->searchTableClientInfo("Client", ClientId);
//    dbManager->printAll(clientInfo);
    clientInfo.next();

    //input currentValue;

    qDebug()<<"FNAme: "<<clientInfo.value(1).toString()<<"MNAme: "<<clientInfo.value(2).toString()<<"LNAME: "<<clientInfo.value(3).toString();
    qDebug()<<"DOB: "<<clientInfo.value(4).toString() <<"GANUM: "<<clientInfo.value(6).toString()<<"SIN: "<<clientInfo.value(7).toString();

    ui->lineEdit_cl_fName->setText(clientInfo.value(1).toString());

    ui->lineEdit_cl_mName->setText(clientInfo.value(2).toString());
    ui->lineEdit_cl_lName->setText(clientInfo.value(3).toString());
    ui->dateEdit_cl_dob->setDate(QDate::fromString(clientInfo.value(4).toString(),"yyyy-MM-dd"));
    //balnace?
    QString caseWorkerName = caseWorkerList.key(clientInfo.value(21).toInt());
    ui->comboBox_cl_caseWorker->setCurrentText(caseWorkerName);
    ui->lineEdit_cl_SIN->setText(clientInfo.value(6).toString());
    ui->lineEdit_cl_GANum->setText(clientInfo.value(7).toString());
    ui->dateEdit_cl_rulesign->setDate(QDate::fromString(clientInfo.value(8).toString(),"yyyy-MM-dd"));

    //NEXT OF KIN FIELD
    ui->lineEdit_cl_nok_name->setText(clientInfo.value(9).toString());
    ui->lineEdit_cl_nok_relationship->setText(clientInfo.value(10).toString());
    ui->lineEdit_cl_nok_loc->setText(clientInfo.value(11).toString());
    ui->lineEdit_cl_nok_ContactNo->setText(clientInfo.value(12).toString());

    //Physician
    ui->lineEdit_cl_phys_name->setText(clientInfo.value(13).toString());
    ui->lineEdit_cl_phys_ContactNo->setText(clientInfo.value(14).toString());

    //Supporter
    ui->lineEdit_cl_supporter_Name->setText(clientInfo.value(15).toString());
    ui->lineEdit_cl_supporter_ContactNo->setText(clientInfo.value(16).toString());
    ui->lineEdit_cl_supporter2_Name->setText(clientInfo.value(22).toString());
    ui->lineEdit_cl_supporter2_ContactNo->setText(clientInfo.value(23).toString());

    ui->comboBox_cl_status->setCurrentText(clientInfo.value(17).toString());

    //comments
    ui->plainTextEdit_cl_comments->clear();
    ui->plainTextEdit_cl_comments->setPlainText(clientInfo.value(18).toString());


    //picture
    QByteArray data = clientInfo.value(20).toByteArray();
    QImage profile = QImage::fromData(data, "PNG");
    addPic(profile);

}

//Client information input and register click
void MainWindow::on_button_register_client_clicked()
{

    if (MainWindow::check_client_register_form())
    {
        QStringList registerFieldList, logFieldList;
        MainWindow::getListRegisterFields(&registerFieldList);
        MainWindow::getRegisterLogFields(&logFieldList);
        if(!dbManager->insertClientLog(&logFieldList))
            return;

        if(registerType == NEWCLIENT || ui->label_cl_infoedit_title->text() == "Register Client")
        {

            if (dbManager->insertClientWithPic(&registerFieldList, &profilePic))
            {
                statusBar()->showMessage("Client Registered Sucessfully.");
                qDebug() << "Client registered successfully";

                clear_client_register_form();
                ui->stackedWidget->setCurrentIndex(1);
            }
            else
            {
                statusBar()->showMessage("Register Failed. Check information.");
                qDebug() << "Could not register client";
            }
        }
        else
        {
            if (dbManager->updateClientWithPic(&registerFieldList, curClientID, &profilePic))
            {
                statusBar()->showMessage("Client information updated Sucessfully.");
                qDebug() << "Client info edit successfully";
                clear_client_register_form();
                ui->stackedWidget->setCurrentIndex(1);
            }
            else
            {
                statusBar()->showMessage("Register Failed. Check information.");
                qDebug() << "Could not edit client info";
            }
        }

    }
    else
    {
        statusBar()->showMessage("Register Failed. Check information.");
        qDebug() << "Register form check was false";
    }
}


//check if the value is valid or not
bool MainWindow::check_client_register_form(){
    if(ui->lineEdit_cl_fName->text().isEmpty()
            && ui->lineEdit_cl_mName->text().isEmpty()
            && ui->lineEdit_cl_lName->text().isEmpty()){
        statusBar()->showMessage(QString("Please Enter Name of Clients"), 5000);
        QPalette pal;
        pal.setColor(QPalette::Normal, QPalette::WindowText, Qt::red);
        ui->label_cl_fName->setPalette(pal);
        ui->label_cl_mName->setPalette(pal);
        ui->label_cl_lName->setPalette(pal);
        return false;
    }

    return true;
}

void MainWindow::getCaseWorkerList(){
    QString caseWorkerquery = "SELECT Username, EmpId FROM Employee WHERE Role = 'CASE WORKER' ORDER BY Username";
    QSqlQuery caseWorkers = dbManager->execQuery(caseWorkerquery);
    //dbManager->printAll(caseWorkers);
    while(caseWorkers.next()){
        qDebug()<<"CASEWORKER: " <<caseWorkers.value(0).toString() << caseWorkers.value(1).toString();
        caseWorkerList.insert(caseWorkers.value(0).toString(), caseWorkers.value(1).toInt());
    }
}

void MainWindow::defaultRegisterOptions(){
    //add caseWorker Name

    if(ui->comboBox_cl_caseWorker->findText("NONE")==-1){
        ui->comboBox_cl_caseWorker->addItem("NONE");
    }

    if(caseWorkerUpdated){
      if(ui->comboBox_cl_caseWorker->count() != caseWorkerList.count()+1){
          QMap<QString, int>::const_iterator it = caseWorkerList.constBegin();
          while(it != caseWorkerList.constEnd()){
            if(ui->comboBox_cl_caseWorker->findText(it.key())== -1){
                ui->comboBox_cl_caseWorker->addItem(it.key());
            }
            ++it;
          }
        }
        caseWorkerUpdated = false;
    }
    if(ui->comboBox_cl_status->findText("Green")==-1){
        ui->comboBox_cl_status->addItem("Green");
        ui->comboBox_cl_status->addItem("Yellow");
        ui->comboBox_cl_status->addItem("Red");
    }

}

void MainWindow::on_button_cancel_client_register_clicked()
{
    clear_client_register_form();
    ui->stackedWidget->setCurrentIndex(MAINMENU);
}


/*==============================================================================
SEARCH CLIENTS USING NAME
==============================================================================*/
//search client
void MainWindow::on_pushButton_search_client_clicked()
{
    qDebug() <<"START SEARCH CLIENT";
    ui->tabWidget_cl_info->setCurrentIndex(0);
    QString clientName = ui->lineEdit_search_clientName->text();

    useProgressDialog("Search Client "+clientName,  QtConcurrent::run(this, &searchClientListThread));
    statusBar()->showMessage(QString("Found " + QString::number(maxClient) + " Clients"), 5000);

    connect(ui->tableWidget_search_client, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(selected_client_info(int,int)),Qt::UniqueConnection);
}

void MainWindow::searchClientListThread(){

    QString clientName = ui->lineEdit_search_clientName->text();
    qDebug()<<"NAME: "<<clientName;
    QSqlQuery resultQ = dbManager->searchClientList(clientName);
    setup_searchClientTable(resultQ);
}

void MainWindow::on_checkBox_search_anonymous_clicked(bool checked)
{
    if(checked){
        QSqlQuery resultQ = dbManager->searchClientList("anonymous");
        setup_searchClientTable(resultQ);
     //  ui->lineEdit_search_clientName->
    }
    else
    {
        qDebug()<<"anonymous check not";
        initClientLookupInfo();
    }
    ui->pushButton_search_client->setEnabled(!checked);
}

void MainWindow::initClientLookupTable(){
    ui->tableWidget_search_client->setRowCount(0);

    ui->tableWidget_search_client->setColumnCount(6);
    ui->tableWidget_search_client->clear();

    ui->tableWidget_search_client->setHorizontalHeaderLabels(QStringList()<<"ClientID"<<"FirstName"<<"Middle Initial"<<"LastName"<<"DateOfBirth"<<"Balance");



}

//set up table widget to add result of search client using name
void MainWindow::setup_searchClientTable(QSqlQuery results){
    //initClientLookupInfo();

    ui->tableWidget_search_client->setRowCount(0);
     int colCnt = results.record().count();
    ui->tableWidget_search_client->setColumnCount(colCnt);
    ui->tableWidget_search_client->clear();

    ui->tableWidget_search_client->setHorizontalHeaderLabels(QStringList()<<"ClientID"<<"FirstName"<<"Middle Initial"<<"LastName"<<"DateOfBirth"<<"Balance");


    int row =0;
    while(results.next()){
        ui->tableWidget_search_client->insertRow(row);
        for(int i =0; i<colCnt; i++){
            if (i == colCnt - 1)
            {
                QString balance = QString("%1%2").arg(results.value(i).toDouble() >= 0 ? "$" : "-$").
                    arg(QString::number(fabs(results.value(i).toDouble()), 'f', 2));
                ui->tableWidget_search_client->
                    setItem(row, i, new QTableWidgetItem(balance));                
            }
            else
            {
                ui->tableWidget_search_client->setItem(row, i, new QTableWidgetItem(results.value(i).toString()));

            }
            //qDebug() <<"row : "<<row << ", col: " << i << "item" << results.value(i).toString();
        }
        row++;
    }

    maxClient = row;
    //ui->tableWidget_search_client->show();

}



//client information tab
void MainWindow::on_tabWidget_cl_info_currentChanged(int index)
{
    switch(index){
        case 0:
            qDebug()<<"Client information tab";
            break;

        case 1:
            if(curClientID == NULL || !newTrans)
                break;
            if(transacFuture.isRunning()|| !transacFuture.isFinished()){
                qDebug()<<"TransactionHistory Is RUNNING";
                return;
            }
            ui->pushButton_cl_trans_more->setEnabled(true);
            transacFuture = QtConcurrent::run(this, &searchTransaction, curClientID);
            useProgressDialog("Read recent transaction...", transacFuture);
            //transacFuture.waitForFinished();
            if(ui->tableWidget_transaction->rowCount()>= transacTotal)
                ui->pushButton_cl_trans_more->setEnabled(false);
            qDebug()<<"client Transaction list";
            newTrans = false;
            break;

       case 2:
            if(curClientID == NULL || !newHistory)
                break;
             if(bookHistoryFuture.isRunning()|| !bookHistoryFuture.isFinished()){
                 qDebug()<<"BookingHistory Is RUNNING";
                 return;
             }
             ui->pushButton_cl_book_more->setEnabled(true);
             bookHistoryFuture = QtConcurrent::run(this, &searchBookHistory, curClientID);
             useProgressDialog("Read recent booking...", bookHistoryFuture);
             //bookHistoryFuture.waitForFinished();
             if(ui->tableWidget_booking->rowCount() >= bookingTotal)
                 ui->pushButton_cl_book_more->setEnabled(false);
             newHistory = false;
             break;
    }
}


//get client information after searching
//double click client information
void MainWindow::selected_client_info(int nRow, int nCol)
{
    Q_UNUSED(nCol)
    curClientID = ui->tableWidget_search_client->item(nRow, 0)->text();
    newTrans = true;
    newHistory = true;
    getClientInfo();
    initClTransactionTable();
    initClBookHistoryTable();
}

void MainWindow::getClientInfo(){
    if(displayFuture.isRunning()|| !displayFuture.isFinished()){
        qDebug()<<"ProfilePic Is RUNNING";
        return;
        //displayFuture.cancel();
    }
    if(displayPicFuture.isRunning() || !displayPicFuture.isFinished()){
        qDebug()<<"ProfilePic Is RUNNING";
         return;
       // displayPicFuture.cancel();
    }
    ui->tabWidget_cl_info->setCurrentIndex(0);
    transacNum = 5;
    bookingNum = 5;
    transacTotal = dbManager->countInformationPerClient("Transac", curClientID);
    bookingTotal = dbManager->countInformationPerClient("booking", curClientID);


    displayFuture = QtConcurrent::run(this, &displayClientInfoThread, curClientID);
    useProgressDialog("Read Client Information...", displayFuture);
    //displayFuture.waitForFinished();
    statusColor();
    displayPicFuture = QtConcurrent::run(this, &displayPicThread);
    useProgressDialog("Read Client Picture...", displayPicFuture);
    addInfoPic(profilePic);
    //displayPicFuture.waitForFinished();

    //useProgressDialog("Read Client Profile Picture...", displayPicFuture);
}

//change status background color after reading client information
void MainWindow::statusColor(){
    QPalette pal(ui->label_cl_info_status->palette());
    QString clStatus = ui->label_cl_info_status->text().toLower();
    if(clStatus == "green"){
        ui->label_cl_info_status->setText("Good Standing");
        pal.setColor(QPalette::Normal, QPalette::Background, Qt::green);
    }else if(clStatus == "yellow"){
        ui->label_cl_info_status->setText("Restricted Access");
        pal.setColor(QPalette::Normal, QPalette::Background, Qt::yellow);
    }else if(clStatus == "red"){
        ui->label_cl_info_status->setText("NO Access");
        pal.setColor(QPalette::Normal, QPalette::Background, Qt::red);
    }else{
        ui->label_cl_info_status->setText("");
        ui->label_cl_info_status->setAutoFillBackground(false);
        return;
    }
    ui->label_cl_info_status->setPalette(pal);
    ui->label_cl_info_status->setAutoFillBackground(true);
}

//delete client info picture  (not use currently)
void MainWindow::clientSearchedInfo(){
    QGraphicsScene *scene = new QGraphicsScene();
    scene->clear();
    ui->graphicsView_getInfo->setScene(scene);

}

//thread function to add client information to the form
void MainWindow::displayClientInfoThread(QString val){

    qDebug()<<"DISPLAY THREAD: " <<val;

    QSqlQuery clientInfo = dbManager->searchClientInfo(val);

   clientInfo.next();

   QString balance = QString("%1%2").arg(clientInfo.value(4).toDouble() >= 0 ? "$" : "-$").
                    arg(QString::number(fabs(clientInfo.value(4).toDouble()), 'f', 2));

   ui->label_cl_info_fName_val->setText(clientInfo.value(0).toString());
   ui->label_cl_info_mName_val->setText(clientInfo.value(1).toString());
   ui->label_cl_info_lName_val->setText(clientInfo.value(2).toString());
   ui->label_cl_info_dob_val->setText(clientInfo.value(3).toString());
   ui->label_cl_info_balance_amt->setText(balance);
   ui->label_cl_info_sin_val->setText(clientInfo.value(5).toString());
   ui->label_cl_info_gaNum_val->setText(clientInfo.value(6).toString());
   QString caseWorkerName = caseWorkerList.key(clientInfo.value(7).toInt());
   ui->label_cl_info_caseWorker_val->setText(caseWorkerName);
   ui->label_cl_info_ruleSignDate_val->setText(clientInfo.value(8).toString());
   ui->label_cl_info_status->setText(clientInfo.value(9).toString());

   ui->label_cl_info_nok_name_val->setText(clientInfo.value(10).toString());
   ui->label_cl_info_nok_relationship_val->setText(clientInfo.value(11).toString());
   ui->label_cl_info_nok_loc_val->setText(clientInfo.value(12).toString());
   ui->label_cl_info_nok_contatct_val->setText(clientInfo.value(13).toString());

   ui->label_cl_info_phys_name_val->setText(clientInfo.value(14).toString());
   ui->label_cl_info_phys_contact_val->setText(clientInfo.value(15).toString());

   ui->label_cl_info_Supporter_name_val->setText(clientInfo.value(16).toString());
   ui->label_cl_info_Supporter_contact_val->setText(clientInfo.value(17).toString());

   ui->label_cl_info_Supporter2_name_val->setText(clientInfo.value(18).toString());
   ui->label_cl_info_Supporter2_contact_val->setText(clientInfo.value(19).toString());
   ui->label_cl_info_comment->setText(clientInfo.value(20).toString());
}

void MainWindow::displayPicThread()
{
    qDebug()<<"displayPicThread";

    if(!dbManager->searchClientInfoPic(&profilePic, curClientID)){
            qDebug()<<"ERROR to get pic";
            return;
        }
    qDebug()<<"Add picture";

//    addInfoPic(profilePic);
}

//add image to client info picture graphicview
void MainWindow::addInfoPic(QImage img){
    qDebug()<<"Add Info Picture??";
    QPixmap item2 = QPixmap::fromImage(img);
    QPixmap scaled = QPixmap(item2.scaledToWidth((int)(ui->graphicsView_getInfo->width()*0.9), Qt::SmoothTransformation));
    QGraphicsScene *scene2 = new QGraphicsScene();
    scene2->addPixmap(QPixmap(scaled));
    ui->graphicsView_getInfo->setScene(scene2);
    ui->graphicsView_getInfo->show();
}

//create new client for booking
void MainWindow::setSelectedClientInfo(){
    //transacTotal
    curClient = new Client();
    int nRow = ui->tableWidget_search_client->currentRow();
    if (nRow <=0){
        if(curClientID == NULL){
            statusBar()->showMessage(tr("Please search and select Client"), 5000);
            return;
        }
        else{
            curClient->clientId = curClientID;
            curClient->fName = ui->label_cl_info_fName_val->text();
            curClient->mName = ui->label_cl_info_mName_val->text();
            curClient->lName =  ui->label_cl_info_lName_val->text();
            QString balanceString = ui->label_cl_info_balance_amt->text();
            balanceString.replace("$", "");
            curClient->balance =  balanceString.toFloat();
            // curClient->balance =  ui->label_cl_info_balance_amt->text().toFloat();

            curClient->fullName = QString(curClient->fName + " " + curClient->mName + " " + curClient->lName);
        }
    }
    else{
        curClientID = curClient->clientId = ui->tableWidget_search_client->item(nRow, 0)->text();
        curClient->fName =  ui->tableWidget_search_client->item(nRow, 1)->text();
        curClient->mName =  ui->tableWidget_search_client->item(nRow, 2)->text();
        curClient->lName =  ui->tableWidget_search_client->item(nRow, 3)->text();
        QString balanceString = ui->tableWidget_search_client->item(nRow, 5)->text();
        balanceString.replace("$", "");
        curClient->balance =  balanceString.toFloat();
        // curClient->balance =  ui->tableWidget_search_client->item(nRow, 5)->text().toFloat();

        curClient->fullName = QString(curClient->fName + " " + curClient->mName + " " + curClient->lName);
    }



    qDebug()<<"ID: " << curClientID << curClient->clientId;
    qDebug()<<"NAME: " << curClient->fullName;
    qDebug()<<"Balance: " << curClient->balance;

    ui->stackedWidget->setCurrentIndex(BOOKINGLOOKUP);


}

//search transaction list when click transaction list
void MainWindow::searchTransaction(QString clientId){
    qDebug()<<"search transaction STaRt";

    QSqlQuery transQuery = dbManager->searchClientTransList(transacNum, clientId, CLIENTLOOKUP);
    initClTransactionTable();
    displayTransaction(transQuery, ui->tableWidget_transaction);

    QString totalNum= (transacTotal == 0)? "-" :
                     (QString::number(ui->tableWidget_transaction->rowCount()) + " / " + QString::number(transacTotal));
    ui->label_cl_trans_total_num->setText(totalNum + " Transaction");


}

void MainWindow::initClTransactionTable(){
    ui->tableWidget_transaction->setRowCount(0);

    ui->tableWidget_transaction->setColumnCount(8);
    ui->tableWidget_transaction->clear();

    ui->tableWidget_transaction->setHorizontalHeaderLabels(QStringList()<<"Date"<<"Time"<<"Amount"<<"Payment Type"<<"ChequeNo"<<"MSQ"<<"ChequeDate"<<"TransType"<<"Employee");
    ui->tableWidget_transaction->setMinimumHeight(30*6-1);

}


//search transaction list when click transaction list
void MainWindow::displayTransaction(QSqlQuery results, QTableWidget* table){

    int row = 0;
    int colCnt = results.record().count();
    while(results.next()){
        table->insertRow(row);
        for(int i =0; i<colCnt; i++){
            if(i == 2){
                QString balance = QString("%1%2").arg(results.value(i).toDouble() >= 0 ? "$" : "-$").
                    arg(QString::number(fabs(results.value(i).toDouble()), 'f', 2));
                table->setItem(row, i, new QTableWidgetItem(balance));
            }
            else
                table->setItem(row, i, new QTableWidgetItem(results.value(i).toString()));
            //qDebug() <<"row : "<<row << ", col: " << i << "item" << results.value(i).toString();
        }

        row++;
    }

    if( table == ui->tableWidget_casefile_transaction)
        return;
    while(row > transacTotal){
        table->setRowCount(transacTotal);
    }
    if (row > 25){
        return;
    }
    table->setMinimumHeight(30*(row+1) -1);
}
//get more transaction list for client info tab
void MainWindow::on_pushButton_cl_trans_more_clicked()
{
    transacNum +=5;
    searchTransaction(curClientID);
    if(ui->tableWidget_transaction->rowCount() >= transacTotal)
        ui->pushButton_cl_trans_more->setEnabled(false);
}

//search booking history when click booking history tab
void MainWindow::searchBookHistory(QString clientId){
    qDebug()<<"search booking";

    QSqlQuery bookingQuery = dbManager->searchBookList(bookingNum, clientId, CLIENTLOOKUP);
    displayBookHistory(bookingQuery, ui->tableWidget_booking);
   // dbManager->printAll(bookingQuery);

    QString totalNum = (bookingTotal == 0)? "-" :
                        QString::number(ui->tableWidget_booking->rowCount()) + " / " + QString::number(bookingTotal);
    ui->label_cl_booking_total_num->setText(totalNum + " Booking");
}

//initialize client booking history table in client info tab
void MainWindow::initClBookHistoryTable(){
    ui->tableWidget_booking->setRowCount(0);
    ui->tableWidget_booking->setColumnCount(6);
    ui->tableWidget_booking->clear();
    ui->tableWidget_booking->setHorizontalHeaderLabels(QStringList()<<"Reserved Date"<<"Program Code"<<"Start Date"<< "End Date"<<"Cost"<<"Bed Number");
    ui->tableWidget_booking->setMinimumHeight(30*6-1);
}


//display booking history in the table view
void MainWindow::displayBookHistory(QSqlQuery results, QTableWidget * table){
    initClBookHistoryTable();
    int colCnt = results.record().count();
    int row =table->rowCount();
    while(results.next()){
        table->insertRow(row);
        for(int i =0; i<colCnt; i++){
            if(i == 4){
                QString balance = QString("%1%2").arg(results.value(i).toDouble() >= 0 ? "$" : "-$").
                    arg(QString::number(fabs(results.value(i).toDouble()), 'f', 2));
                table->setItem(row, i, new QTableWidgetItem(balance));
            }
            else
                table->setItem(row, i, new QTableWidgetItem(results.value(i).toString()));
            //qDebug() <<"row : "<<row << ", col: " << i << "item" << results.value(i).toString();
        }
        row++;
    }
    if(table == ui->tableWidget_casefile_booking)
        return;
    if(row > bookingTotal)
        table->setRowCount(bookingTotal);

    if (row > 25){
        table->setMinimumHeight(30*26 -1);
        return;
    }
    if(row >5)
        table->setMinimumHeight(30*(row+1) -1);
}

//click more client button fore booking history
void MainWindow::on_pushButton_cl_book_more_clicked()
{
    bookingNum +=5;
    searchBookHistory(curClientID);
    if(ui->tableWidget_booking->rowCount() >= bookingTotal )
        ui->pushButton_cl_book_more->setEnabled(false);
}

//DELETE CLIENT INFORMATION FIELDS
void MainWindow::initClientLookupInfo(){
    //init client search table
    if(ui->tableWidget_search_client->columnCount()>0){
        ui->tableWidget_search_client->setColumnCount(0);
        ui->tableWidget_search_client->clear();
        ui->lineEdit_search_clientName->clear();
    }

    //init client Info Form Field
    ui->label_cl_info_fName_val->clear();
    ui->label_cl_info_mName_val->clear();
    ui->label_cl_info_lName_val->clear();
    ui->label_cl_info_dob_val->clear();
    ui->label_cl_info_balance_amt->clear();
    ui->label_cl_info_sin_val->clear();
    ui->label_cl_info_gaNum_val->clear();
    ui->label_cl_info_caseWorker_val->clear();
    ui->label_cl_info_ruleSignDate_val->clear();
    ui->label_cl_info_status->clear();

    ui->label_cl_info_nok_name_val->clear();
    ui->label_cl_info_nok_relationship_val->clear();
    ui->label_cl_info_nok_loc_val->clear();
    ui->label_cl_info_nok_contatct_val->clear();

    ui->label_cl_info_phys_name_val->clear();
    ui->label_cl_info_phys_contact_val->clear();

    ui->label_cl_info_Supporter_name_val->clear();
    ui->label_cl_info_Supporter_contact_val->clear();
    ui->label_cl_info_Supporter2_name_val->clear();
    ui->label_cl_info_Supporter2_contact_val->clear();

    ui->label_cl_info_comment->clear();

    QGraphicsScene *scene = new QGraphicsScene();
    scene->clear();
    ui->graphicsView_getInfo->setScene(scene);

    profilePic = (QImage)NULL;

    ui->label_cl_info_status->setAutoFillBackground(false);

    //initialize transaction
    initClTransactionTable();

    //initialize transaction total count
    transacTotal = 0;
    ui->label_cl_trans_total_num->setText("");

    //initialize booking history table
    bookingTotal = 0;
    initClBookHistoryTable();

    //initialize booking total count
    ui->label_cl_booking_total_num->setText("");

    //disable buttons that need a clientId
    if(curClientID == NULL){
        ui->pushButton_bookRoom->setEnabled(false);
        ui->pushButton_processPaymeent->setEnabled(false);
        ui->pushButton_editClientInfo->setEnabled(false);
        ui->pushButton_CaseFiles->setEnabled(false);
    }

    //hide buttons for different workflows
    switch (workFlow){
    case BOOKINGPAGE:
        ui->pushButton_CaseFiles->setVisible(false);
        ui->pushButton_processPaymeent->setVisible(false);
        ui->pushButton_bookRoom->setVisible(true);
        ui->horizontalSpacer_79->changeSize(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed);
        ui->horizontalSpacer_80->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_81->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        break;
    case PAYMENTPAGE:
        ui->pushButton_CaseFiles->setVisible(false);
        ui->pushButton_bookRoom->setVisible(false);
        ui->pushButton_processPaymeent->setVisible(true);
        ui->horizontalSpacer_79->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_80->changeSize(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed);
        ui->horizontalSpacer_81->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        break;
    case CASEFILE:
        ui->pushButton_CaseFiles->setVisible(true);
        ui->pushButton_bookRoom->setVisible(false);
        ui->pushButton_processPaymeent->setVisible(false);
        ui->horizontalSpacer_79->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_80->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_81->changeSize(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed);
        break;
    case CLIENTLOOKUP:
        ui->pushButton_CaseFiles->setVisible(true);
        ui->pushButton_processPaymeent->setVisible(true);
        ui->pushButton_bookRoom->setVisible(true);
        ui->horizontalSpacer_79->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_80->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalSpacer_81->changeSize(1,1,QSizePolicy::Fixed,QSizePolicy::Fixed);
        ui->horizontalLayout_15->update();
        break;
    }


}


/////////////////////////////////////////////////////////////////////////////////


// the add user button
void MainWindow::on_btn_createNewUser_clicked()
{
    // temporary disable stuff
    // obtain username and pw and role from UI
    QString uname = ui->le_userName->text();
    QString pw = ui->le_password->text();

    if (uname.length() == 0) {
        ui->lbl_editUserWarning->setText("Enter a Username");
        return;
    }

    if (pw.length() == 0) {
        ui->lbl_editUserWarning->setText("Enter a Password");
        return;
    }

    // first, check to see if the username is taken
    QSqlQuery queryResults = dbManager->findUser(uname);
    int numrows = queryResults.numRowsAffected();

    if (numrows > 0) {
        ui->lbl_editUserWarning->setText("This username is already taken");
        return;
    } else {
        QSqlQuery queryResults = dbManager->addNewEmployee(uname, pw, ui->comboBox->currentText());
        int numrows = queryResults.numRowsAffected();

        if (numrows != 0) {
            ui->lbl_editUserWarning->setText("Employee added");
            QStandardItemModel * model = new QStandardItemModel(0,0);
            model->clear();
            ui->tableWidget_3->clear();
            ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_3->setColumnCount(3);
            ui->tableWidget_3->setRowCount(0);
            ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");

            ui->comboBox->setCurrentIndex(0);
            ui->le_userName->setText("");
            ui->le_password->setText("");
            ui->le_users->setText("");
        } else {
            ui->lbl_editUserWarning->setText("Something went wrong - please try again");
        }
    }
}


void MainWindow::on_btn_dailyReport_clicked()
{
    ui->swdg_reports->setCurrentIndex(DAILYREPORT);
}

void MainWindow::on_btn_shiftReport_clicked()
{
    ui->swdg_reports->setCurrentIndex(SHIFTREPORT);
}

void MainWindow::on_btn_floatCount_clicked()
{
    ui->swdg_reports->setCurrentIndex(FLOATCOUNT);
}

void MainWindow::on_confirmationFinal_clicked()
{
    curBook = 0;
    curClient = 0;
    trans = 0;
    ui->stackedWidget->setCurrentIndex(MAINMENU);
}



void MainWindow::on_btn_listAllUsers_clicked()
{
    ui->tableWidget_3->setRowCount(0);
    ui->tableWidget_3->clear();
    ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT Username, Password, Role FROM Employee");

    int numCols = result.record().count();
    ui->tableWidget_3->setColumnCount(numCols);
    ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        ui->tableWidget_3->insertRow(x);
        QStringList row;
        row << result.value(0).toString() << result.value(1).toString() << result.value(2).toString();
        for (int i = 0; i < 3; ++i)
        {
            ui->tableWidget_3->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }
}

void MainWindow::on_btn_searchUsers_clicked()
{
    QString ename = ui->le_users->text();
    ui->tableWidget_3->setRowCount(0);
    ui->tableWidget_3->clear();
    ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT Username, Password, Role FROM Employee WHERE Username LIKE '%"+ ename +"%'");

    int numCols = result.record().count();
    ui->tableWidget_3->setColumnCount(numCols);
    ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        ui->tableWidget_3->insertRow(x);
        QStringList row;
        row << result.value(0).toString() << result.value(1).toString() << result.value(2).toString();
        for (int i = 0; i < 3; ++i)
        {
            ui->tableWidget_3->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }


//    QSqlQuery results = dbManager->execQuery("SELECT Username, Password, Role FROM Employee WHERE Username LIKE '%"+ ename +"%'");
//    QSqlQueryModel *model = new QSqlQueryModel();
//    model->setQuery(results);

////    ui->tableWidget_3->setModel(model);
////    ui->tableWidget_3->horizontalHeader()->model()->setHeaderData(0, Qt::Horizontal, "Username");
////    ui->tableWidget_3->horizontalHeader()->model()->setHeaderData(1, Qt::Horizontal, "Password");
////    ui->tableWidget_3->horizontalHeader()->model()->setHeaderData(2, Qt::Horizontal, "Role");
}





// double clicked employee
void MainWindow::on_tableWidget_3_doubleClicked(const QModelIndex &index)
{
    // populate the fields on the right
    QString uname = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 0)).toString();
    QString pw = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 1)).toString();
    QString role = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 2)).toString();
    qDebug() << uname;
    qDebug() << pw;
    qDebug() << role;

//    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableWidget_3->model());
//    int row = index.row();

//     QStandardItemModel* model = ui->tableWidget_3->model();
//    qDebug() << model;
//    QString uname = model->item(row, 0)->text();
//    QString pw = model->item(row, 1)->text();
//    QString role = model->item(row, 2)->text();

    if (role == "STANDARD") {
        ui->comboBox->setCurrentIndex(0);
    } else if (role == "CASE WORKER") {
        ui->comboBox->setCurrentIndex(1);
    } else if (role == "ADMIN") {
        ui->comboBox->setCurrentIndex(2);
    }

    ui->le_userName->setText(uname);
    ui->le_password->setText(pw);
}

void MainWindow::on_pushButton_CaseFiles_clicked()
{
    addHistory(CLIENTLOOKUP);
    setSelectedClientInfo();
    ui->stackedWidget->setCurrentIndex(CASEFILE);

    double width = ui->tw_pcpRela->size().width();

    for (auto x: pcp_tables){
        x->resizeRowsToContents();
        x->setColumnWidth(0, width*0.41);
        x->setColumnWidth(1, width*0.41);
        x->setColumnWidth(2, width*0.16);
    }

    //get client id
    int nRow = ui->tableWidget_search_client->currentRow();
    if (nRow <0)
        return;
    curClientID = ui->tableWidget_search_client->item(nRow, 0)->text();
    QString curFirstName = ui->tableWidget_search_client->item(nRow, 1)->text();
    QString curMiddleName = ui->tableWidget_search_client->item(nRow, 2)->text().size() > 0 ? ui->tableWidget_search_client->item(nRow, 2)->text()+ " " : "";
    QString curLastName = ui->tableWidget_search_client->item(nRow, 3)->text();

    qDebug() << "id displayed:" << idDisplayed;
    qDebug() << "id selected:" << curClientID;
    if (idDisplayed != curClientID) {
        idDisplayed = curClientID;
        ui->lbl_caseClientName->setText(curFirstName + " " + curMiddleName + curLastName + "'s Case Files");
        populateCaseFiles();
    }
}

void MainWindow::populateCaseFiles(QString type, int tableId) {

    //running notes
    ui->te_notes->clear();
    QSqlQuery noteResult = dbManager->readNote(curClientID);
    qDebug() << noteResult.lastError();
    while (noteResult.next()) {
        ui->te_notes->document()->setPlainText(noteResult.value(0).toString());
    }

    //pcp tables
    int tableIdx = 0;
    if (type == "all") {

        for (auto x: pcpTypes) {
            QString query = "SELECT rowId, Goal, Strategy, Date "
                            "FROM Pcp "
                            "WHERE ClientId = " + curClientID +
                            " AND Type = '" + x + "'";
            QSqlQuery result = dbManager->execQuery(query);
            qDebug() << result.lastError();
            int numRows = result.numRowsAffected();
            auto table = (pcp_tables.at(tableIdx++));

            //reset table
            table->clearContents();
            table->setMinimumHeight(73);
            table->setMaximumHeight(1);
            table->setMaximumHeight(16777215);
            table->setRowCount(1);

            //set number of rows
            for (int i = 0; i < numRows-1; i++) {
                table->insertRow(0);

                //set height of table
                table->setMinimumHeight(table->minimumHeight() + 35);
            }

            //populate table
            while (result.next()){
                for (int i = 0; i < 3; i++) {
                    table->setItem(result.value(0).toString().toInt(), i, new QTableWidgetItem(result.value(i+1).toString()));
                }
            }
        }
        return;
    } else {

        QString query = "SELECT rowId, Goal, Strategy, Date "
                        "FROM Pcp "
                        "WHERE ClientId = " + curClientID +
                        " AND Type = '" + type + "'";
        QSqlQuery result = dbManager->execQuery(query);

        qDebug() << result.lastError();
        int numRows = result.numRowsAffected();
        auto table = (pcp_tables.at(tableId));

        //reset table
        table->clearContents();
        table->setMinimumHeight(73);
        table->setMaximumHeight(1);
        table->setMaximumHeight(16777215);
        table->setRowCount(1);

        //set number of rows
        for (int i = 0; i < numRows-1; i++) {
            table->insertRow(0);

            //set height of table
            table->setMinimumHeight(table->minimumHeight() + 35);
        }

        //populate table
        while (result.next()){
            for (int i = 0; i < 3; i++) {
                table->setItem(result.value(0).toString().toInt(), i, new QTableWidgetItem(result.value(i+1).toString()));
            }
        }
    }
}
//CASEFILE WIDGET CHANGE CONTROL
void MainWindow::on_tabw_casefiles_currentChanged(int index)
{
    switch(index)
    {
        case PERSIONACASEPLAN:
            ui->actionExport_to_PDF->setEnabled(true);
            break;
        case RUNNINGNOTE:
            ui->actionExport_to_PDF->setEnabled(true);
            break;
        case BOOKINGHISTORY:
            ui->actionExport_to_PDF->setEnabled(false);
            if(!newHistory)
                break;
            on_pushButton_casefile_book_reload_clicked();
            /*
            initCasefileBookHistoryTable();
            useProgressDialog("Search Transaction...",QtConcurrent::run(this, &searchCasefileBookHistory, curClientID));
            bookingTotal = ui->tableWidget_casefile_booking->rowCount();
            ui->label_casefile_booking_total_num->setText(QString::number(bookingTotal) + " Booking");
            */
            newHistory = false;
            break;

        case TRANSACTIONHISTORY:
            ui->actionExport_to_PDF->setEnabled(false);
            if(!newTrans)
                break;
            on_pushButton_casefile_trans_reload_clicked();
            /*
            initCasefileTransactionTable();
            useProgressDialog("Search Transaction...",QtConcurrent::run(this, &searchCasefileTransaction, curClientID));
            transacTotal = ui->tableWidget_casefile_transaction->rowCount();
            ui->label_casefile_trans_total_num->setText(QString::number(transacTotal) + " Transaction");
            */
            newTrans = false;
            break;
    }
}

/*--------------------------------------------------
        CASEFILE BOOKING HISTORY(EUNWON)
----------------------------------------------------*/
//search booking history when click booking history tab
void MainWindow::initCasefileBookHistoryTable(){
    ui->tableWidget_casefile_booking->setRowCount(0);
    ui->tableWidget_casefile_booking->setColumnCount(8);
    ui->tableWidget_casefile_booking->clear();
    ui->tableWidget_casefile_booking->setHorizontalHeaderLabels(QStringList()<<"Reserved Date"<<"Program Code"<<"Start Date"<<"End Date"<<"Cost"<<"Bed Number"<<"FirstBook"<<"Monthly");
}

void MainWindow::searchCasefileBookHistory(QString clientId){
    qDebug()<<"search booking";

    QSqlQuery bookingQuery = dbManager->searchBookList(bookingNum, clientId, CASEFILE);
    displayBookHistory(bookingQuery, ui->tableWidget_casefile_booking);


}

void MainWindow::on_pushButton_casefile_book_reload_clicked()
{
    initCasefileBookHistoryTable();
    ui->tableWidget_casefile_booking->setSortingEnabled(false);
    useProgressDialog("Search Transaction...",QtConcurrent::run(this, &searchCasefileBookHistory, curClientID));
    bookingTotal = ui->tableWidget_casefile_booking->rowCount();
    ui->label_casefile_booking_total_num->setText(QString::number(bookingTotal) + " Booking");
    ui->tableWidget_casefile_booking->sortByColumn(3, Qt::DescendingOrder);
    ui->tableWidget_casefile_booking->setSortingEnabled(true);
}


/*-----------------------------------------------
        CASEFILE TRANSACTION(EUNWON)
-------------------------------------------------*/
void MainWindow::initCasefileTransactionTable(){
    ui->tableWidget_casefile_transaction->setRowCount(0);

    ui->tableWidget_casefile_transaction->setColumnCount(12);
    ui->tableWidget_casefile_transaction->clear();
    ui->tableWidget_casefile_transaction->setHorizontalHeaderLabels(QStringList()<<"Date"<<"Time"<<"Amount"<<"Payment Type"<<"ChequeNo"<<"MSQ"<<"ChequeDate"<<"TransType"
                                                           <<"Deleted"<<"Outstanding"<<"Employee"<<"Notes");

}

//search transaction list when click transaction list
void MainWindow::searchCasefileTransaction(QString clientId){
    qDebug()<<"search transaction STaRt";
    initCasefileTransactionTable();
    QSqlQuery transQuery = dbManager->searchClientTransList(transacNum, clientId, CASEFILE);
    displayTransaction(transQuery, ui->tableWidget_casefile_transaction);
}

//reload client transaction history
void MainWindow::on_pushButton_casefile_trans_reload_clicked()
{
    initCasefileTransactionTable();
    ui->tableWidget_casefile_transaction->setSortingEnabled(false);
    useProgressDialog("Search Transaction...",QtConcurrent::run(this, &searchCasefileTransaction, curClientID));
    transacTotal = ui->tableWidget_casefile_transaction->rowCount();
    ui->label_casefile_trans_total_num->setText(QString::number(transacTotal) + " Transaction");
    ui->tableWidget_casefile_transaction->sortByColumn(0, Qt::DescendingOrder);
    ui->tableWidget_casefile_transaction->setSortingEnabled(true);
}


/*-------------------CASEFILE END-----------------------------------*/


void MainWindow::on_EditRoomsButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(EDITROOM);
    addHistory(ADMINPAGE);

    // set dropdowns
    populate_modRoom_cboxes();

    qDebug() << "pushed page " << ADMINPAGE;
}

// update employee button
void MainWindow::on_pushButton_4_clicked()
{
    // obtain username and pw and role from UI
    QString uname = ui->le_userName->text();
    QString pw = ui->le_password->text();

    if (uname.length() == 0) {
        ui->lbl_editUserWarning->setText("Enter a Username");
        return;
    }

    if (pw.length() == 0) {
        ui->lbl_editUserWarning->setText("Enter a Password");
        return;
    }

    // first, check to make sure the username is taken
    QSqlQuery queryResults = dbManager->findUser(uname);
    int numrows1 = queryResults.numRowsAffected();

    if (numrows1 == 1) {
        QSqlQuery queryResults = dbManager->updateEmployee(uname, pw, ui->comboBox->currentText());
        int numrows = queryResults.numRowsAffected();

        if (numrows != 0) {
            ui->lbl_editUserWarning->setText("Employee Updated");
            QStandardItemModel * model = new QStandardItemModel(0,0);
            model->clear();
            ui->tableWidget_3->clear();
            ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_3->setColumnCount(3);
            ui->tableWidget_3->setRowCount(0);
            ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");

            ui->comboBox->setCurrentIndex(0);
            ui->le_userName->setText("");
            ui->le_password->setText("");
            ui->le_users->setText("");
        } else {
            ui->lbl_editUserWarning->setText("Something went wrong - Please try again");
        }

        return;
    } else {
        ui->lbl_editUserWarning->setText("Employee Not Found");
        return;
    }
}

// Clear button
void MainWindow::on_btn_displayUser_clicked()
{
    QStandardItemModel * model = new QStandardItemModel(0,0);
    model->clear();
    ui->tableWidget_3->clear();
    ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_3->setColumnCount(3);
    ui->tableWidget_3->setRowCount(0);
    ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");

    ui->comboBox->setCurrentIndex(0);
    ui->le_userName->setText("");
    ui->le_password->setText("");
    ui->le_users->setText("");
}

void MainWindow::on_tableWidget_3_clicked(const QModelIndex &index)
{
    // populate the fields on the right
    QString uname = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 0)).toString();
    QString pw = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 1)).toString();
    QString role = ui->tableWidget_3->model()->data(ui->tableWidget_3->model()->index(index.row(), 2)).toString();
    qDebug() << uname;
    qDebug() << pw;
    qDebug() << role;

//    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableWidget_3->model());
//    int row = index.row();

//     QStandardItemModel* model = ui->tableWidget_3->model();
//    qDebug() << model;
//    QString uname = model->item(row, 0)->text();
//    QString pw = model->item(row, 1)->text();
//    QString role = model->item(row, 2)->text();

    if (role == "STANDARD") {
        ui->comboBox->setCurrentIndex(0);
    } else if (role == "CASE WORKER") {
        ui->comboBox->setCurrentIndex(1);
    } else if (role == "ADMIN") {
        ui->comboBox->setCurrentIndex(2);
    }

    ui->le_userName->setText(uname);
    ui->le_password->setText(pw);
}

// delete button
void MainWindow::on_pushButton_6_clicked()
{
    QString uname = ui->le_userName->text();
    QString pw = ui->le_password->text();

    if (uname.length() == 0) {
        ui->lbl_editUserWarning->setText("Please make sure a valid employee is selected");
        return;
    }

    if (pw.length() == 0) {
        ui->lbl_editUserWarning->setText("Please make sure a valid employee is selected");
        return;
    }

    QSqlQuery queryResults = dbManager->findUser(uname);
    int numrows1 = queryResults.numRowsAffected();

    if (numrows1 == 1) {
        QSqlQuery queryResults = dbManager->deleteEmployee(uname, pw, ui->comboBox->currentText());
        int numrows = queryResults.numRowsAffected();

        if (numrows != 0) {
            ui->lbl_editUserWarning->setText("Employee Deleted");
            QStandardItemModel * model = new QStandardItemModel(0,0);
            model->clear();
            ui->tableWidget_3->clear();
            ui->tableWidget_3->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_3->setColumnCount(3);
            ui->tableWidget_3->setRowCount(0);
            ui->tableWidget_3->setHorizontalHeaderLabels(QStringList() << "Username" << "Password" << "Role");

            ui->comboBox->setCurrentIndex(0);
            ui->le_userName->setText("");
            ui->le_password->setText("");
            ui->le_users->setText("");
        } else {
            ui->lbl_editUserWarning->setText("Employee Not Found");
        }

        return;
    } else {
        ui->lbl_editUserWarning->setText("Employee Not Found");
        return;
    }

}

// list all rooms
void MainWindow::on_btn_listAllUsers_3_clicked()
{
    QString ename = ui->le_users_3->text();
    ui->tableWidget_5->setRowCount(0);
    ui->tableWidget_5->clear();
    ui->tableWidget_5->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT SpaceCode, cost, Monthly FROM Space ORDER BY SpaceCode");

//    int numCols = result.record().count();
    ui->tableWidget_5->setColumnCount(8);
    ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
    int x = 0;
    int qt = result.size();
    qDebug() << "<" << qt;
    while (result.next()) {
        // break down the spacecode

        QString spacecode = result.value(0).toString();
        if (spacecode == "") {
            break;
        }
        std::string strspacecode = spacecode.toStdString();

        std::vector<std::string> brokenupspacecode = split(strspacecode, '-');
        // parse space code to check building number + floor number + room number + space number
        QString buildingnum = QString::fromStdString(brokenupspacecode[0]);
        QString floornum = QString::fromStdString(brokenupspacecode[1]);
        QString roomnum = QString::fromStdString(brokenupspacecode[2]);
        std::string bednumtype = brokenupspacecode[3];

        qDebug() << "Spacecode type:" << QString::fromStdString(brokenupspacecode[3]);

        // strip the last character
        QString bednumber = QString::fromStdString(bednumtype.substr(0, bednumtype.size()-1));

        qDebug() << bednumber;


        // get the last character to figure out the type
        char typechar = bednumtype[bednumtype.size() - 1];

        qDebug() << typechar;

        QString thetype = "" + typechar;

        qDebug() << thetype;

        ui->tableWidget_5->insertRow(x);
        QStringList row;
        row << spacecode
            << buildingnum
            << floornum
            << roomnum
            << bednumber
            << QChar::fromLatin1(typechar)
            << result.value(1).toString()
            << result.value(2).toString();
        for (int i = 0; i < 8; ++i)
        {
            ui->tableWidget_5->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }
}

// list all programs
void MainWindow::on_btn_listAllUsers_2_clicked()
{
    ui->tableWidget_2->setRowCount(0);
    ui->tableWidget_2->clear();
    ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT ProgramCode, Description FROM Program");

    int numCols = result.record().count();
    ui->tableWidget_2->setColumnCount(numCols);
    ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "Program Code" << "Description");
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        ui->tableWidget_2->insertRow(x);
        QStringList row;
        row << result.value(0).toString() << result.value(1).toString();
        for (int i = 0; i < 2; ++i)
        {
            ui->tableWidget_2->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }
}

// search programs by code
void MainWindow::on_btn_searchUsers_2_clicked()
{
    QString ename = ui->le_users_2->text();
    ui->tableWidget_2->setRowCount(0);
    ui->tableWidget_2->clear();
    ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT ProgramCode, Description FROM Program WHERE ProgramCode LIKE '%"+ ename +"%'");

    int numCols = result.record().count();
    ui->tableWidget_2->setColumnCount(numCols);
    ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "Program Code" << "Description");
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        ui->tableWidget_2->insertRow(x);
        QStringList row;
        row << result.value(0).toString() << result.value(1).toString();
        for (int i = 0; i < 2; ++i)
        {
            ui->tableWidget_2->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }
}

// delete program
void MainWindow::on_pushButton_25_clicked()
{
    QString pcode = ui->le_userName_2->text();

    if (pcode.length() == 0) {
        ui->lbl_editUserWarning->setText("Please make sure a valid Program is selected");
        return;
    }

    QSqlQuery queryResults = dbManager->execQuery("DELETE FROM Program WHERE ProgramCode='" + pcode + "'");
    int numrows = queryResults.numRowsAffected();

    if (numrows != 0) {
        ui->lbl_editProgWarning->setText("Program Deleted");
        QStandardItemModel * model = new QStandardItemModel(0,0);
        model->clear();
        ui->tableWidget_2->clear();
        ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
        ui->tableWidget_2->setColumnCount(2);
        ui->tableWidget_2->setRowCount(0);
        ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "Program Code" << "Description");

        ui->comboBox->setCurrentIndex(0);
        ui->le_userName->setText("");
        ui->le_password->setText("");
        ui->le_users->setText("");
    } else {
        ui->lbl_editProgWarning->setText("Program Not Found");
    }
    return;
}

// program clicked + selected
void MainWindow::on_tableWidget_2_clicked(const QModelIndex &index)
{
    if (lastprogramclicked != index) {
        ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
        ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
        ui->lbl_editProgWarning->setText("Please hold while we set your beds");
        qApp->processEvents();

        ui->availablebedslist->clear();
        ui->availablebedslist->setRowCount(0);
        ui->assignedbedslist->clear();
        ui->assignedbedslist->setRowCount(0);

        ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
        ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");

        // populate the fields on the right
        QString pcode = ui->tableWidget_2->model()->data(ui->tableWidget_2->model()->index(index.row(), 0)).toString();
        QString description = ui->tableWidget_2->model()->data(ui->tableWidget_2->model()->index(index.row(), 1)).toString();

        ui->le_userName_2->setText(pcode);
        ui->textEdit->setText(description);

        // populate the beds list
        QSqlQuery availSpaces = dbManager->getAvailableBeds(pcode);
        int numrowsavail = availSpaces.numRowsAffected();
        if (numrowsavail > 0) {
//            int numCols = availSpaces.record().count();
            ui->availablebedslist->setColumnCount(1);
            int x = 0;
            int qt = availSpaces.size();
            qDebug() << qt;
            while (availSpaces.next()) {
                ui->availablebedslist->insertRow(x);
                QStringList row;
                QString buildingNo = availSpaces.value(0).toString();
                QString floorNo = availSpaces.value(1).toString();
                QString roomNo = availSpaces.value(2).toString();
                QString spaceNo = availSpaces.value(4).toString();
                QString type = availSpaces.value(5).toString();

                //1-319-3
                QString roomCode = buildingNo + "-" + floorNo + "-" + roomNo + "-" + spaceNo + type[0];

                ui->availablebedslist->setItem(x, 0, new QTableWidgetItem(roomCode));
                x++;
            }
        }
        ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
        ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
        qApp->processEvents();

    //    QStandardItemModel* availmodel = new QStandardItemModel();

    //    while (availSpaces.next()) {
    //        QString buildingNo = availSpaces.value(0).toString();
    //        QString floorNo = availSpaces.value(1).toString();
    //        QString roomNo = availSpaces.value(2).toString();
    //        QString spaceNo = availSpaces.value(4).toString();
    //        QString type = availSpaces.value(5).toString();

    //        //1-319-3
    //        QString roomCode = buildingNo + "-" + floorNo + "-" + roomNo + "-" + spaceNo + type[0];

    //        // QStandardItem* Items = new QStandardItem(availSpaces.value(1).toString());
    //        QStandardItem* Items = new QStandardItem(roomCode);
    //        availmodel->appendRow(Items);
    //    }

    //    ui->availablebedslist->setModel(availmodel);

          QSqlQuery assignedspaces = dbManager->getAssignedBeds(pcode);
          int numrowsassigned = assignedspaces.numRowsAffected();
          if (numrowsassigned > 0) {
//              int numCols = assignedspaces.record().count();
              ui->assignedbedslist->setColumnCount(1);
              int x = 0;
              int qt = assignedspaces.size();
              qDebug() << qt;
              while (assignedspaces.next()) {
                  ui->assignedbedslist->insertRow(x);
                  QStringList row;
                  QString buildingNo = assignedspaces.value(0).toString();
                  QString floorNo = assignedspaces.value(1).toString();
                  QString roomNo = assignedspaces.value(2).toString();
                  QString spaceNo = assignedspaces.value(4).toString();
                  QString type = assignedspaces.value(5).toString();

                  //1-319-3
                  QString roomCode = buildingNo + "-" + floorNo + "-" + roomNo + "-" + spaceNo + type[0];

                  ui->assignedbedslist->setItem(x, 0, new QTableWidgetItem(roomCode));
                  x++;
              }
          }
          ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
          ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
          ui->lbl_editProgWarning->setText("");

    //    int numrowsassigned = assignedspaces.numRowsAffected();

    //    QStandardItemModel* assignedmodel = new QStandardItemModel();

    //    while (assignedspaces.next()) {
    //        QString buildingNo = assignedspaces.value(0).toString();
    //        QString floorNo = assignedspaces.value(1).toString();
    //        QString roomNo = assignedspaces.value(2).toString();
    //        QString spaceNo = assignedspaces.value(4).toString();
    //        QString type = assignedspaces.value(5).toString();

    //        //1-319-3
    //        QString roomCode = buildingNo + "-" + floorNo + "-" + roomNo + "-" + spaceNo + type[0];

    //        // QStandardItem* Items = new QStandardItem(availSpaces.value(1).toString());
    //        QStandardItem* Items = new QStandardItem(roomCode);
    //        assignedmodel->appendRow(Items);
    //    }

        // ui->assignedbedslist->setModel(assignedmodel);
          lastprogramclicked = index;
    }

}

// set case files directory
void MainWindow::on_pushButton_3_clicked()
{
    const QString DEFAULT_DIR_KEY("default_dir");
    QSettings mruDir;
    QString tempDir = QFileDialog::getExistingDirectory(
                    this,
                    tr("Select Directory"),
                    mruDir.value(DEFAULT_DIR_KEY).toString()
                );
    if (!tempDir.isEmpty()) {
        dir = tempDir;
        mruDir.setValue(DEFAULT_DIR_KEY, tempDir);
        int nRow = ui->tableWidget_search_client->currentRow();

        QStringList filter = (QStringList() << "*" + ui->tableWidget_search_client->item(nRow, 3)->text() + ", " +
                              ui->tableWidget_search_client->item(nRow, 1)->text() + "*");

        qDebug() << "*" + ui->tableWidget_search_client->item(nRow, 3)->text() + ", " +
                    ui->tableWidget_search_client->item(nRow, 1)->text() + "*";
        ui->le_caseDir->setText(dir.path());
        populate_tw_caseFiles(filter);
    }
    connect(ui->tw_caseFiles, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(on_tw_caseFiles_cellDoubleClicked(int,int)), Qt::UniqueConnection);
}

// open case file in external reader
void MainWindow::on_tw_caseFiles_cellDoubleClicked(int row, int column)
{
    qDebug() << QUrl::fromLocalFile(dir.absoluteFilePath(ui->tw_caseFiles->item(row, column)->text())) << "at" << row << " " << column;
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absoluteFilePath(ui->tw_caseFiles->item(row, column)->text())));
}

// filter file names
void MainWindow::on_btn_caseFilter_clicked()
{
    qDebug() << "filter button clicked, filter with" << ui->le_caseFilter->text();
    QStringList filter = (QStringList() << "*"+(ui->le_caseFilter->text())+"*");
    populate_tw_caseFiles(filter);
}

void MainWindow::populate_tw_caseFiles(QStringList filter){
    int i = 0;
    dir.setNameFilters(filter);
    for (auto x : dir.entryList(QDir::Files)) {
        qDebug() << x;
        ui->tw_caseFiles->setRowCount(i+1);
        ui->tw_caseFiles->setItem(i++,0, new QTableWidgetItem(x));
//        ui->tw_caseFiles->resizeColumnsToContents();
        ui->tw_caseFiles->resizeRowsToContents();
    }
    if (i > 0) {
        ui->btn_caseFilter->setEnabled(true);
    }
}

// create new program button
void MainWindow::on_btn_createNewUser_2_clicked()
{
    QString pcode = ui->le_userName_2->text();
    QString pdesc = ui->textEdit->toPlainText();

    if (pcode.length() == 0) {
        ui->lbl_editProgWarning->setText("Please Enter a Program Code");
        return;
    }

    if (pdesc.length() == 0) {
        ui->lbl_editProgWarning->setText("Please Enter a Program Description");
        return;
    }

    QSqlQuery queryResults = dbManager->execQuery("SELECT * FROM Program WHERE ProgramCode='" + pcode + "'");
    int numrows = queryResults.numRowsAffected();

    if (numrows == 1) {
        ui->lbl_editProgWarning->setText("Program code in use");
        return;
    } else {
        QSqlQuery qr = dbManager->AddProgram(pcode, pdesc);
        if (qr.numRowsAffected() == 1) {
            ui->lbl_editProgWarning->setText("Program Added");
            QStandardItemModel * model = new QStandardItemModel(0,0);
            model->clear();
            ui->tableWidget_2->clear();
            ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_2->setColumnCount(2);
            ui->tableWidget_2->setRowCount(0);
            ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "Program Code" << "Description");

            ui->comboBox->setCurrentIndex(0);
            ui->le_userName->setText("");
            ui->le_password->setText("");
            ui->le_users->setText("");
        } else {
            ui->lbl_editProgWarning->setText("Program not added - please try again.");
        }
    }
}

// update program
void MainWindow::on_pushButton_24_clicked()
{
    QString pcode = ui->le_userName_2->text();
    QString pdesc = ui->textEdit->toPlainText();

    if (pcode.length() == 0) {
        ui->lbl_editProgWarning->setText("Please Enter a Program Code");
        return;
    }

    if (pdesc.length() == 0) {
        ui->lbl_editProgWarning->setText("Please Enter a Program Description");
        return;
    }

    QSqlQuery queryResults = dbManager->execQuery("SELECT * FROM Program WHERE ProgramCode='" + pcode + "'");
    int numrows = queryResults.numRowsAffected();

    if (numrows != 1) {
        ui->lbl_editProgWarning->setText("Enter a valid program code to update");
        return;
    } else {
        QSqlQuery qr = dbManager->updateProgram(pcode, pdesc);
        if (qr.numRowsAffected() == 1) {
            ui->lbl_editProgWarning->setText("Program Updated");
            QStandardItemModel * model = new QStandardItemModel(0,0);
            model->clear();
            ui->tableWidget_2->clear();
            ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
            ui->tableWidget_2->setColumnCount(2);
            ui->tableWidget_2->setRowCount(0);
            ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "Program Code" << "Description");

            ui->comboBox->setCurrentIndex(0);
            ui->le_userName->setText("");
            ui->le_password->setText("");
            ui->le_users->setText("");
        } else {
            ui->lbl_editProgWarning->setText("Program not updated - please try again.");
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event);
    double width = ui->tw_pcpRela->size().width();
    for (auto x: pcp_tables){
        x->resizeRowsToContents();
        x->setColumnWidth(0, width*0.41);
        x->setColumnWidth(1, width*0.41);
        x->setColumnWidth(2, width*0.16);
    }
}

//void MainWindow::on_tw_pcpRela_itemChanged(QTableWidgetItem *item)
//{
//    for (auto x: pcp_tables){
//        x->resizeRowsToContents();
//    }
//}

void MainWindow::initPcp(){
    pcp_tables.push_back(ui->tw_pcpRela);
    pcp_tables.push_back(ui->tw_pcpEdu);
    pcp_tables.push_back(ui->tw_pcpSub);
    pcp_tables.push_back(ui->tw_pcpAcco);
    pcp_tables.push_back(ui->tw_pcpLife);
    pcp_tables.push_back(ui->tw_pcpMent);
    pcp_tables.push_back(ui->tw_pcpPhy);
    pcp_tables.push_back(ui->tw_pcpLeg);
    pcp_tables.push_back(ui->tw_pcpAct);
    pcp_tables.push_back(ui->tw_pcpTrad);
    pcp_tables.push_back(ui->tw_pcpOther);
    pcp_tables.push_back(ui->tw_pcpPpl);

    pcpTypes = {
                    "relationship",
                    "educationEmployment",
                    "substanceUse",
                    "accomodationsPlanning",
                    "lifeSkills",
                    "mentalHealth",
                    "physicalHealth",
                    "legalInvolvement",
                    "activities",
                    "traditions",
                    "other",
                    "people"
                };
}

void MainWindow::on_btn_pcpRela_clicked()
{
    ui->tw_pcpRela->insertRow(0);
    ui->tw_pcpRela->setMinimumHeight(ui->tw_pcpRela->minimumHeight()+35);
}

void MainWindow::on_btn_pcpEdu_clicked()
{
    ui->tw_pcpEdu->insertRow(0);
    ui->tw_pcpEdu->setMinimumHeight(ui->tw_pcpEdu->minimumHeight()+35);
}

void MainWindow::on_btn_pcpSub_clicked()
{
    ui->tw_pcpSub->insertRow(0);
    ui->tw_pcpSub->setMinimumHeight(ui->tw_pcpSub->minimumHeight()+35);
}

void MainWindow::on_btn_pcpAcc_clicked()
{
    ui->tw_pcpAcco->insertRow(0);
    ui->tw_pcpAcco->setMinimumHeight(ui->tw_pcpAcco->minimumHeight()+35);
}

void MainWindow::on_btn_pcpLife_clicked()
{
    ui->tw_pcpLife->insertRow(0);
    ui->tw_pcpLife->setMinimumHeight(ui->tw_pcpLife->minimumHeight()+35);
}

void MainWindow::on_btn_pcpMent_clicked()
{
    ui->tw_pcpMent->insertRow(0);
    ui->tw_pcpMent->setMinimumHeight(ui->tw_pcpMent->minimumHeight()+35);
}

void MainWindow::on_btn_pcpPhy_clicked()
{
    ui->tw_pcpPhy->insertRow(0);
    ui->tw_pcpPhy->setMinimumHeight(ui->tw_pcpPhy->minimumHeight()+35);
}

void MainWindow::on_btn_pcpLeg_clicked()
{
    ui->tw_pcpLeg->insertRow(0);
    ui->tw_pcpLeg->setMinimumHeight(ui->tw_pcpLeg->minimumHeight()+35);
}

void MainWindow::on_btn_pcpAct_clicked()
{
    ui->tw_pcpAct->insertRow(0);
    ui->tw_pcpAct->setMinimumHeight(ui->tw_pcpAct->minimumHeight()+35);
}

void MainWindow::on_btn_pcpTrad_clicked()
{
    ui->tw_pcpTrad->insertRow(0);
    ui->tw_pcpTrad->setMinimumHeight(ui->tw_pcpTrad->minimumHeight()+35);
}

void MainWindow::on_btn_pcpOther_clicked()
{
    ui->tw_pcpOther->insertRow(0);
    ui->tw_pcpOther->setMinimumHeight(ui->tw_pcpOther->minimumHeight()+35);
}

void MainWindow::on_btn_pcpKey_clicked()
{
    ui->tw_pcpPpl->insertRow(0);
    ui->tw_pcpPpl->setMinimumHeight(ui->tw_pcpPpl->minimumHeight()+35);
}

void MainWindow::on_addbedtoprogram_clicked()
{
    // add program tag to bed
    QString pcode = ui->le_userName_2->text();

    // QModelIndexList qil = ui->availablebedslist->selectedIndexes();

    // get selected bed
    qDebug() << ui->availablebedslist->currentItem()->text();
    std::string selectedtag = ui->availablebedslist->currentItem()->text().toStdString();

    std::vector<std::string> tagbreakdown = split(selectedtag, '-');

    // parse space code to check building number + floor number + room number + space number
    QString buildingnum = QString::fromStdString(tagbreakdown[0]);
    QString floornum = QString::fromStdString(tagbreakdown[1]);
    QString roomnum = QString::fromStdString(tagbreakdown[2]);
    std::string bednumtype = tagbreakdown[3];
    // strip the last character
    QString bednumber = QString::fromStdString(bednumtype.substr(0, bednumtype.size()-1));

    // get space id
    QSqlQuery singlebedquery = dbManager->searchSingleBed(buildingnum, floornum, roomnum, bednumber);
    singlebedquery.next();
    QString spaceid = singlebedquery.value(3).toString();

    // get curr tag
    QString currenttag = singlebedquery.value(8).toString();

    // append to tag
    currenttag += ",";
    currenttag += pcode;

    qDebug() << currenttag;

    // update tag value
    dbManager->updateSpaceProgram(spaceid, currenttag);

    ui->availablebedslist->clear();
    ui->availablebedslist->setRowCount(0);
    ui->assignedbedslist->clear();
    ui->assignedbedslist->setRowCount(0);
    ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
    ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
    ui->lbl_editProgWarning->setText("Bed Added to Program");

//    const auto selectedIdxs = ui->availablebedslist->selectedIndexes();
//    if (!selectedIdxs.isEmpty()) {
//        const QVariant var = ui->availablebedslist->model()->data(selectedIdxs.first());
//        // next you need to convert your `var` from `QVariant` to something that you show from your data with default (`Qt::DisplayRole`) role, usually it is a `QString`:
//        const QString selectedItemString = var.toString();

//        // or you also may do this by using `QStandardItemModel::itemFromIndex()` method:
//        const QStandardItem* selectedItem = dynamic_cast<QStandardItemModel*>(model())->itemFromIndex(selectedIdxs.first());
//        // use your `QStandardItem`
//    }

    // populate the beds list
//    QSqlQuery availSpaces = dbManager->getAvailableBeds(pcode);
//    int numrowsavail = availSpaces.numRowsAffected();

//    QStandardItemModel* availmodel = new QStandardItemModel();

//    while (availSpaces.next()) {
//        QString buildingNo = availSpaces.value(0).toString();
//        QString floorNo = availSpaces.value(1).toString();
//        QString roomNo = availSpaces.value(2).toString();
//        QString spaceNo = availSpaces.value(4).toString();
//        QString type = availSpaces.value(5).toString();

//        //1-319-3
//        QString roomCode = buildingNo + "-" + floorNo + "-" + roomNo + "-" + spaceNo + type[0];

//        // QStandardItem* Items = new QStandardItem(availSpaces.value(1).toString());
//        QStandardItem* Items = new QStandardItem(roomCode);
//        availmodel->appendRow(Items);
//    }

//    ui->availablebedslist->setModel(availmodel);


}

void MainWindow::on_removebedfromprogram_clicked()
{
    // remove program tag from bed
    QString pcode = ui->le_userName_2->text();

    // get selected bed
    qDebug() << ui->assignedbedslist->currentItem()->text();
    std::string selectedtag = ui->assignedbedslist->currentItem()->text().toStdString();

    std::vector<std::string> tagbreakdown = split(selectedtag, '-');

    // parse space code to check building number + floor number + room number + space number
    QString buildingnum = QString::fromStdString(tagbreakdown[0]);
    QString floornum = QString::fromStdString(tagbreakdown[1]);
    QString roomnum = QString::fromStdString(tagbreakdown[2]);
    std::string bednumtype = tagbreakdown[3];
    // strip the last character
    QString bednumber = QString::fromStdString(bednumtype.substr(0, bednumtype.size()-1));

    // get space id
    QSqlQuery singlebedquery = dbManager->searchSingleBed(buildingnum, floornum, roomnum, bednumber);
    singlebedquery.next();
    QString spaceid = singlebedquery.value(3).toString();

    // get curr tag
    QString currenttag = singlebedquery.value(8).toString();

    // remove tag
    QString newtag = "";
    std::string curtagtogether = currenttag.toStdString();
    std::vector<std::string> curtagbreakdown = split(curtagtogether, ',');
    for (std::string t : curtagbreakdown) {
        if (QString::fromStdString(t) != pcode) {
            newtag += QString::fromStdString(t);
            newtag += ",";
        }
    }
    std::string tempstr = newtag.toStdString();
    tempstr.pop_back();
    newtag = QString::fromStdString(tempstr);

    qDebug() << newtag;

    // update tag value
    dbManager->updateSpaceProgram(spaceid, newtag);
    ui->availablebedslist->clear();
    ui->availablebedslist->setRowCount(0);
    ui->assignedbedslist->clear();
    ui->assignedbedslist->setRowCount(0);
    ui->availablebedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
    ui->assignedbedslist->setHorizontalHeaderLabels(QStringList() << "Bed Code");
    ui->lbl_editProgWarning->setText("Bed Removed from Program");
}

void MainWindow::on_availablebedslist_clicked(const QModelIndex &index)
{
    // clicked available bed
//    availableBedIndex = index;
//    QStringList list;
//    QStandardItemModel model = (QStandardItemModel) ui->availablebedslist->model();
//    foreach(const QModelIndex &index, ui->availablebedslist->selectionModel()->selectedIndexes()) {
//        list.append(model->itemFromIndex(index)->text());
//    }
}

void MainWindow::on_assignedbedslist_clicked(const QModelIndex &index)
{
    // clicked assigned bed
    assignedBedIndex = index;
}

void MainWindow::on_btn_monthlyReport_clicked()
{
    ui->swdg_reports->setCurrentIndex(MONTHLYREPORT);
}


void MainWindow::on_btn_restrictedList_clicked()
{
    ui->swdg_reports->setCurrentIndex(RESTRICTIONS);
}


/*==============================================================================
REPORTS
==============================================================================*/
void MainWindow::setupReportsScreen()
{
    // Daily Report Screen
    ui->lbl_dailyReportDateVal->setText(QDate::currentDate().toString(Qt::ISODate));
    ui->dailyReport_dateEdit->setDate(QDate::currentDate());
    checkoutReport = new Report(this, ui->checkout_tableView, CHECKOUT_REPORT);
    vacancyReport = new Report(this, ui->vacancy_tableView, VACANCY_REPORT);
    lunchReport = new Report(this, ui->lunch_tableView, LUNCH_REPORT);
    wakeupReport = new Report(this, ui->wakeup_tableView, WAKEUP_REPORT);

    // Shift Report Screen
    ui->lbl_shiftReportDateVal->setText(QDate::currentDate().toString(Qt::ISODate));
    ui->lbl_shiftReportShiftVal->setText(QString::number(currentshiftid));
    ui->shiftReport_dateEdit->setDate(QDate::currentDate());
    ui->shiftReport_spinBox->setValue(currentshiftid);

    bookingReport = new Report(this, ui->booking_tableView, BOOKING_REPORT);
    transactionReport = new Report(this, ui->transaction_tableView, TRANSACTION_REPORT);
    clientLogReport = new Report(this, ui->clientLog_tableView, CLIENT_REPORT);
    otherReport = new Report(this, ui->other_tableView, OTHER_REPORT);
    yellowReport = new Report(this, ui->yellowRestriction_tableView, YELLOW_REPORT);
    redReport = new Report(this, ui->redRestriction_tableView, RED_REPORT);

    ui->saveOther_btn->setEnabled(false);

    // Float Report Screen
    ui->cashFloatDate_lbl->setText(QDate::currentDate().toString(Qt::ISODate));
    ui->cashFloatShift_lbl->setText(QString::number(currentshiftid));
    ui->cashFloat_dateEdit->setDate(QDate::currentDate());
    ui->cashFloat_spinBox->setValue(currentshiftid);

    // Monthly Report Screen
    QStringList monthList;
    monthList << "Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun" << "Jul" << "Aug" << "Sep" << "Oct" << "Nov" << "Dec";
    ui->month_comboBox->addItems(monthList);

    QStringList yearList;
    for (int i = 2016; i <= QDate::currentDate().year(); i++)
    {
        yearList << QString::number(i);
    }
    ui->year_comboBox->addItems(yearList);

    ui->month_comboBox->setCurrentIndex(QDate::currentDate().month() - 1);
    ui->year_comboBox->setCurrentIndex(yearList.size() - 1);
}

void MainWindow::updateDailyReportTables(QDate date)
{
    // useProgressDialog("Processing reports...",
    //     QtConcurrent::run(checkoutReport, &checkoutReport->updateModelThread, date));
    checkoutReport->updateModel(date);
    vacancyReport->updateModel(date);
    lunchReport->updateModel(date);
    wakeupReport->updateModel(date);

    ui->lbl_dailyReportDateVal->setText(date.toString(Qt::ISODate));
    ui->dailyReport_dateEdit->setDate(date);
}

void MainWindow::updateRestrictionTables()
{
    useProgressDialog("Processing reports...",
        QtConcurrent::run(yellowReport, &yellowReport->updateModelThread));
    //yellowReport->updateModel();
    redReport->updateModel();

    ui->restrictionLastUpdated_lbl->setText(QString(QDate::currentDate().toString(Qt::ISODate)
        + " at " + QTime::currentTime().toString("h:mmAP")));
}

void MainWindow::updateShiftReportTables(QDate date, int shiftNo)
{
    // useProgressDialog("Processing reports...",
    //     QtConcurrent::run(bookingReport, &bookingReport->updateModelThread, date, shiftNo));
    bookingReport->updateModel(date, shiftNo);
    transactionReport->updateModel(date, shiftNo);
    clientLogReport->updateModel(date, shiftNo);
    otherReport->updateModel(date, shiftNo);

    ui->lbl_shiftReportDateVal->setText(date.toString(Qt::ISODate));
    ui->lbl_shiftReportShiftVal->setText(QString::number(shiftNo));
    ui->shiftReport_dateEdit->setDate(date);
}

void MainWindow::on_dailyReportGo_btn_clicked()
{
    QDate date = ui->dailyReport_dateEdit->date();

    MainWindow::updateDailyReportTables(date);
    MainWindow::getDailyReportStats(date);
}

void MainWindow::on_dailyReportCurrent_btn_clicked()
{
    ui->dailyReport_dateEdit->setDate(QDate::currentDate());
}

void MainWindow::on_shiftReportGo_btn_clicked()
{
    QDate date = ui->shiftReport_dateEdit->date();
    int shiftNo = ui->shiftReport_spinBox->value();

    MainWindow::updateShiftReportTables(date, shiftNo);
    MainWindow::getShiftReportStats(date, shiftNo);
}

void MainWindow::on_shiftReportCurrent_btn_clicked()
{
    ui->shiftReport_dateEdit->setDate(QDate::currentDate());
    ui->shiftReport_spinBox->setValue(currentshiftid);
}

void MainWindow::on_other_lineEdit_textEdited(const QString &text)
{
    if (text.trimmed().isEmpty())
    {
        ui->saveOther_btn->setEnabled(false);
    }
    else
    {
        ui->saveOther_btn->setEnabled(true);
    }
}

void MainWindow::on_saveOther_btn_clicked()
{
    QString logText = ui->other_lineEdit->text();
    if (dbManager->insertOtherLog(userLoggedIn, currentshiftid, logText))
    {
        statusBar()->showMessage(tr("New log saved"), 5000);
        ui->other_lineEdit->clear();
    }
    else
    {
        statusBar()->showMessage(tr("Could not save new log"), 5000);
    }
}

void MainWindow::getDailyReportStats(QDate date)
{
    useProgressDialog("Processing reports...",
        QtConcurrent::run(dbManager, &DatabaseManager::getDailyReportStatsThread, date));
}

void MainWindow::getShiftReportStats(QDate date, int shiftNo)
{
    useProgressDialog("Processing reports...",
        QtConcurrent::run(dbManager, &DatabaseManager::getShiftReportStatsThread, date, shiftNo));
}

void MainWindow::getCashFloat(QDate date, int shiftNo)
{
    useProgressDialog("Processing reports...",
        QtConcurrent::run(dbManager, &DatabaseManager::getCashFloatThread, date, shiftNo));
}

void MainWindow::getMonthlyReport(int month, int year)
{
    useProgressDialog("Processing reports...",
        QtConcurrent::run(dbManager, &DatabaseManager::getMonthlyReportThread, month, year));
}

void MainWindow::updateDailyReportStats(QList<int> list, bool conn)
{
    if (!conn)
    {
        MainWindow::on_noDatabaseConnection();
    }
    else
    {
        ui->lbl_espCheckouts->setText(QString::number(list.at(0)));
        ui->lbl_totalCheckouts->setText(QString::number(list.at(1)));
        ui->lbl_espVacancies->setText(QString::number(list.at(2)));
        ui->lbl_totalVacancies->setText(QString::number(list.at(3)));
    }
}

void MainWindow::updateShiftReportStats(QStringList list, bool conn)
{
    if (!conn)
    {
        MainWindow::on_noDatabaseConnection();
    }
    else
    {
        ui->lbl_cashAmt->setText(list.at(0));
        ui->lbl_debitAmt->setText(list.at(1));
        ui->lbl_chequeAmt->setText(list.at(2));
        ui->lbl_depoAmt->setText(list.at(3));
        ui->lbl_shiftAmt->setText(list.at(4));    }
}

void MainWindow::updateCashFloat(QDate date, int shiftNo, QStringList list, bool conn)
{
    if (!conn)
    {
        MainWindow::on_noDatabaseConnection();
    }
    else
    {
        ui->lastModifiedEmpName_lbl->setText(list.at(0));
        ui->lastModifiedDateTime_lbl->setText(list.at(1));
        ui->pte_floatMemo->document()->setPlainText(list.at(2));
        ui->sbox_nickels->setValue(list.at(3).toInt());
        ui->sbox_dimes->setValue(list.at(4).toInt());
        ui->sbox_quarters->setValue(list.at(5).toInt());
        ui->sbox_loonies->setValue(list.at(6).toInt());
        ui->sbox_toonies->setValue(list.at(7).toInt());
        ui->sbox_fives->setValue(list.at(8).toInt());
        ui->sbox_tens->setValue(list.at(9).toInt());
        ui->sbox_twenties->setValue(list.at(10).toInt());
        ui->sbox_fifties->setValue(list.at(11).toInt());
        ui->sbox_hundreds->setValue(list.at(12).toInt());
        ui->lbl_floatAmt->setText(list.at(13));

        ui->cashFloatDate_lbl->setText(date.toString(Qt::ISODate));
        ui->cashFloatShift_lbl->setText(QString::number(shiftNo));
    }
}

void MainWindow::on_saveFloat_btn_clicked()
{
    MainWindow::on_calculateTotal_btn_clicked();

    QDate date = ui->cashFloat_dateEdit->date();
    int shiftNo = ui->cashFloat_spinBox->value();
    QString empName = userLoggedIn;
    QString comments = ui->pte_floatMemo->toPlainText();
    QList<int> coins;
    coins << ui->sbox_nickels->value() << ui->sbox_dimes->value() << ui->sbox_quarters->value()
          << ui->sbox_loonies->value() << ui->sbox_toonies->value() << ui->sbox_fives->value()
          << ui->sbox_tens->value() << ui->sbox_twenties->value() << ui->sbox_fifties->value()
          << ui->sbox_hundreds->value();

    useProgressDialog("Saving Cash Float Changes ...", QtConcurrent::run(dbManager,
        &DatabaseManager::insertCashFloat, date, shiftNo, empName, comments, coins));
}

void MainWindow::on_calculateTotal_btn_clicked()
{
    double total = ui->sbox_nickels->value() * VAL_NICKEL;
    total += ui->sbox_dimes->value() * VAL_DIME;
    total += ui->sbox_quarters->value() * VAL_QUARTER;
    total += ui->sbox_loonies->value() * VAL_LOONIE;
    total += ui->sbox_toonies->value() * VAL_TOONIE;
    total += ui->sbox_fives->value() * VAL_FIVE;
    total += ui->sbox_tens->value() * VAL_TEN;
    total += ui->sbox_twenties->value() * VAL_TWENTY;
    total += ui->sbox_fifties->value() * VAL_FIFTY;
    total += ui->sbox_hundreds->value() * VAL_HUNDRED;

    ui->lbl_floatAmt->setText(QString("$ " + QString::number(total, 'f', 2)));
}

void MainWindow::on_cashFloatGo_btn_clicked()
{
    QDate date = ui->cashFloat_dateEdit->date();
    int shiftNo = ui->cashFloat_spinBox->value();

    MainWindow::getCashFloat(date, shiftNo);
}

void MainWindow::on_restrictionRefresh_btn_clicked()
{
    MainWindow::updateRestrictionTables();
}

void MainWindow::on_cashFloatCurrent_btn_clicked()
{
    ui->cashFloat_dateEdit->setDate(QDate::currentDate());
    ui->cashFloat_spinBox->setValue(currentshiftid);
}

void MainWindow::updateCashFloatLastEditedLabels(QString empName,
    QString currentDateStr, QString currentTimeStr)
{
    ui->lastModifiedEmpName_lbl->setText(empName);
    ui->lastModifiedDateTime_lbl->setText(currentDateStr + " at " + currentTimeStr);
}

void MainWindow::on_monthlyReportGo_btn_clicked()
{
    int month = ui->month_comboBox->currentIndex() + 1;
    int year = ui->year_comboBox->currentText().toInt();

    MainWindow::getMonthlyReport(month, year);
}

void MainWindow::updateMonthlyReportUi(QStringList list, bool conn)
{
    if (!conn)
    {
        MainWindow::on_noDatabaseConnection();
    }
    else
    {
        ui->numBedsUsed_lbl->setText(list.at(0) + "/" + list.at(2));
        ui->numEmptyBeds_lbl->setText(list.at(1) + "/" + list.at(2));
        ui->numNewClients_lbl->setText(list.at(3));
        ui->numUniqueClients_lbl->setText(list.at(4));
        QString month = ui->month_comboBox->currentText();
        QString year = ui->year_comboBox->currentText();
        ui->monthlyReportMonth_lbl->setText(QString(month + " " + year));
    }
}

void MainWindow::on_noDatabaseConnection()
{
    statusBar()->showMessage(tr(""));
    QString msg = QString("\nCould not connect to the database.\n")
        + QString("\nPlease remember to save your changes when the connection to the database returns.\n")
        + QString("\nSelect \"File\" followed by the \"Connect to Database\" menu item to attempt to connect to the database.\n");
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("ArcWay");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setText(msg);
    msgBox.exec();
}


void MainWindow::on_noDatabaseConnection(QSqlDatabase* database)
{
    statusBar()->showMessage(tr(""));
    QString msg = QString("\nCould not connect to the database.\n")
        + QString("\nPlease remember to save your changes when the connection to the database returns.\n")
        + QString("\nSelect \"File\" followed by the \"Connect to Database\" menu item to attempt to connect to the database.\n");
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("ArcWay");
    QPushButton* reconnectButton = msgBox.addButton(tr("Re-connect"), QMessageBox::AcceptRole);
    msgBox.setStandardButtons(QMessageBox::Close);
    msgBox.setText(msg);
    msgBox.exec();

    if (msgBox.clickedButton() == reconnectButton)
    {
        statusBar()->showMessage(tr("Re-connecting"));
        dbManager->reconnectToDatabase(database);
    }
    else
    {
        statusBar()->showMessage(tr("No database connection"));
    }
}

void MainWindow::on_reconnectedToDatabase()
{
    statusBar()->showMessage(tr("Database connection established"), 3000);
}
/*==============================================================================
REPORTS (END)
==============================================================================*/
void MainWindow::on_actionReconnect_to_Database_triggered()
{
    statusBar()->showMessage(tr("Re-connecting"));
    dbManager->reconnectToDatabase();
}

void MainWindow::on_actionBack_triggered()
{
    if (!backStack.isEmpty()){
        int page = backStack.pop();
        forwardStack.push(ui->stackedWidget->currentIndex());
        ui->stackedWidget->setCurrentIndex(page);
        ui->actionForward->setEnabled(true);
    }
}

void MainWindow::on_actionForward_triggered()
{
    if (!forwardStack.isEmpty()) {
        int page = forwardStack.pop();
        backStack.push(ui->stackedWidget->currentIndex());
        ui->stackedWidget->setCurrentIndex(page);
    }
}

void MainWindow::addHistory(int n){
    backStack.push(n);
    forwardStack.clear();
    ui->actionBack->setEnabled(true);
    ui->actionForward->setEnabled(false);
}

void MainWindow::on_pushButton_processPaymeent_clicked()
{
    addHistory(CLIENTLOOKUP);
    int nRow = ui->tableWidget_search_client->currentRow();
    if (nRow <0)
        return;

    curClient = new Client();
    curClientID = curClient->clientId = ui->tableWidget_search_client->item(nRow, 0)->text();
    curClient->fName =  ui->tableWidget_search_client->item(nRow, 1)->text();
    curClient->mName =  ui->tableWidget_search_client->item(nRow, 2)->text();
    curClient->lName =  ui->tableWidget_search_client->item(nRow, 3)->text();
    QString balanceString = ui->tableWidget_search_client->item(nRow, 5)->text();
    balanceString.replace("$", "");
    curClient->balance =  balanceString.toFloat();
    // curClient->balance =  ui->tableWidget_search_client->item(nRow, 5)->text().toFloat();
    curClient->fullName = QString(curClient->fName + " " + curClient->mName + " " + curClient->lName);

    trans = new transaction();
    QString note = "";
    payment * pay = new payment(this, trans, curClient->balance, 0 , curClient, note, true, userLoggedIn, QString::number(currentshiftid));
    pay->exec();
    delete(pay);
}

void MainWindow::insertPcp(QTableWidget *tw, QString type){
    QString goal;
    QString strategy;
    QString date;

    int rows = tw->rowCount();
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < 3; j++) {
            if (tw->item(i,j)) {
                switch (j) {
                case 0: goal = tw->item(i,j)->text();
                    break;
                case 1: strategy = tw->item(i,j)->text();
                    break;
                case 2: date = tw->item(i,j)->text();
                }

              }
        }
        QSqlQuery delResult = dbManager->deletePcpRow(i, type);
        qDebug() << delResult.lastError();
        QSqlQuery addResult = dbManager->addPcp(i, curClientID, type, goal, strategy, date);
        qDebug() << addResult.lastError();
    }
}

void MainWindow::on_btn_pcpRelaSave_clicked()
{
    insertPcp(ui->tw_pcpRela, "relationship");
}

void MainWindow::on_btn_pcpEduSave_clicked()
{
    insertPcp(ui->tw_pcpEdu, "educationEmployment");
}

void MainWindow::on_btn_pcpSubSave_clicked()
{
    insertPcp(ui->tw_pcpSub, "substanceUse");
}

void MainWindow::on_btn_pcpAccoSave_clicked()
{
    insertPcp(ui->tw_pcpAcco, "accomodationsPlanning");
}

void MainWindow::on_btn_pcpLifeSave_clicked()
{
    insertPcp(ui->tw_pcpLife, "lifeSkills");
}

void MainWindow::on_btn_pcpMentSave_clicked()
{
    insertPcp(ui->tw_pcpMent, "mentalHealth");
}

void MainWindow::on_btn_pcpPhySave_clicked()
{
    insertPcp(ui->tw_pcpPhy, "physicalHealth");
}

void MainWindow::on_btn_pcpLegSave_clicked()
{
    insertPcp(ui->tw_pcpLeg, "legalInvolvement");
}

void MainWindow::on_btn_pcpActSave_2_clicked()
{
    insertPcp(ui->tw_pcpAct, "activities");
}

void MainWindow::on_btn_pcpTradSave_clicked()
{
    insertPcp(ui->tw_pcpTrad, "traditions");
}

void MainWindow::on_btn_pcpOtherSave_clicked()
{
    insertPcp(ui->tw_pcpOther, "other");
}

void MainWindow::on_btn_pcpKeySave_clicked()
{
    insertPcp(ui->tw_pcpPpl, "people");
}

void MainWindow::on_actionPcptables_triggered()
{
//    QtRPT *report = new QtRPT(this);
//    report->loadReport("clientList");
//    report->recordCount << ui->tableWidget_search_client->rowCount();
//    connect(report, SIGNAL(setValue(const int, const QString, const QString, const QString, const QString)),
//           this, SLOT(setValue(const int, const QString, const QString, const QString, const QString)));
//    report->printExec();

//    for (int i = 0; i < checkoutReport->model.tableData->size(); ++i)
//         qDebug() << checkoutReport->model.tableData->at(i);

    qDebug() << ui->stackedWidget->currentIndex() << " " << ui->dailyReport_tabWidget->currentIndex();

}

void MainWindow::on_btn_pcpRelaUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(0);
    populateCaseFiles(pcpTypes.at(0), 0);
}

void MainWindow::on_btn_pcpEduUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(1);
    populateCaseFiles(pcpTypes.at(1), 1);
}

void MainWindow::on_btn_pcpSubUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(2);
    populateCaseFiles(pcpTypes.at(2), 2);
}

void MainWindow::on_btn_pcpAccoUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(3);
    populateCaseFiles(pcpTypes.at(3), 3);
}

void MainWindow::on_btn_pcpLifeUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(4);
    populateCaseFiles(pcpTypes.at(4), 4);
}

void MainWindow::on_btn_pcpMentUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(5);
    populateCaseFiles(pcpTypes.at(5), 5);
}

void MainWindow::on_btn_pcpPhyUndo_2_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(6);
    populateCaseFiles(pcpTypes.at(6), 6);
}

void MainWindow::on_btn_pcpLegUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(7);
    populateCaseFiles(pcpTypes.at(7), 7);
}

void MainWindow::on_btn_pcpActUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(8);
    populateCaseFiles(pcpTypes.at(8), 8);
}

void MainWindow::on_btn_pcpTradUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(9);
    populateCaseFiles(pcpTypes.at(9), 9);
}

void MainWindow::on_btn_pcpOtherUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(10);
    populateCaseFiles(pcpTypes.at(10), 10);
}

void MainWindow::on_btn_pcpPplUndo_clicked()
{
    qDebug() << "resetting table " << pcpTypes.at(11);
    populateCaseFiles(pcpTypes.at(11), 11);
}

void MainWindow::on_btn_notesSave_clicked()
{
    QString notes = ui->te_notes->toPlainText();
    QSqlQuery result = dbManager->addNote(curClientID, notes);
    if (result.numRowsAffected() == 1) {
//        ui->lbl_noteWarning->setStyleSheet("QLabel#lbl_noteWarning {color = black;}");
//        ui->lbl_noteWarning->setText("Saved");
    } else {
//        ui->lbl_noteWarning->setStyleSheet("QLabel#lbl_noteWarning {color = red;}");
//        ui->lbl_noteWarning->setText(result.lastError().text());
        QSqlQuery result2 = dbManager->updateNote(curClientID, notes);
        qDebug() << result2.lastError();

    }

}

void MainWindow::on_btn_notesUndo_clicked()
{
    QSqlQuery result = dbManager->readNote(curClientID);
    qDebug() << result.lastError();
    while (result.next()) {
        ui->te_notes->document()->setPlainText(result.value(0).toString());
    }
}

void MainWindow::on_btn_searchUsers_3_clicked()
{
    QString ename = ui->le_users_3->text();
    ui->tableWidget_5->setRowCount(0);
    ui->tableWidget_5->clear();
    ui->tableWidget_5->horizontalHeader()->setStretchLastSection(true);

    QSqlQuery result = dbManager->execQuery("SELECT SpaceCode, cost, Monthly FROM Space WHERE SpaceCode LIKE '%"+ ename +"%'");

//    int numCols = result.record().count();
    ui->tableWidget_5->setColumnCount(8);
    ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        // break down the spacecode

        QString spacecode = result.value(0).toString();
        std::string strspacecode = spacecode.toStdString();

        std::vector<std::string> brokenupspacecode = split(strspacecode, '-');
        // parse space code to check building number + floor number + room number + space number
        QString buildingnum = QString::fromStdString(brokenupspacecode[0]);
        QString floornum = QString::fromStdString(brokenupspacecode[1]);
        QString roomnum = QString::fromStdString(brokenupspacecode[2]);
        std::string bednumtype = brokenupspacecode[3];
        // strip the last character
        QString bednumber = QString::fromStdString(bednumtype.substr(0, bednumtype.size()-1));

        // get the last character to figure out the type
        char typechar = bednumtype[bednumtype.size()-1];
        QString thetype = "" + typechar;

    // get space id
//    QSqlQuery singlebedquery = dbManager->searchSingleBed(buildingnum, floornum, roomnum, bednumber);
//    singlebedquery.next();
//    QString spaceid = singlebedquery.value(3).toString();

        ui->tableWidget_5->insertRow(x);
        QStringList row;
        row << spacecode
            << buildingnum
            << floornum
            << roomnum
            << bednumber
            << QChar::fromLatin1(typechar)
            << result.value(1).toString()
            << result.value(2).toString();
        for (int i = 0; i < 8; ++i)
        {
            ui->tableWidget_5->setItem(x, i, new QTableWidgetItem(row.at(i)));
        }
        x++;
    }
}

void MainWindow::populate_modRoom_cboxes() {
    ui->cbox_roomLoc->clear();
    ui->cbox_roomFloor->clear();
    ui->cbox_roomRoom->clear();
    ui->cbox_roomType->clear();

    QSqlQuery buildings = dbManager->execQuery("SELECT BuildingNo FROM Building");
    ui->cbox_roomLoc->addItems(QStringList() << "");
    while (buildings.next()) {
        ui->cbox_roomLoc->addItems(QStringList() << buildings.value(0).toString());
    }
}

void MainWindow::on_btn_modRoomBdlg_clicked()
{
    curmodifyingspace = BUILDINGS;
    ui->editroommodifybox->clear();
    ui->editroommodifybox->setColumnCount(1);
    ui->editroommodifybox->setRowCount(0);
    ui->editroommodifybox->horizontalHeader()->setStretchLastSection(true);
    ui->editroommodifybox->setHorizontalHeaderLabels(QStringList() << "Building Number");

    // get all buildings
    QSqlQuery result = dbManager->execQuery("SELECT BuildingNo FROM Building");

    // populate table
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        QString buildingno = result.value(0).toString();
        ui->editroommodifybox->insertRow(x);
        QStringList row;
        row << buildingno;
        for (int i = 0; i < 1; ++i)
        {
            ui->editroommodifybox->setItem(x, i, new QTableWidgetItem(row.at(i)));
            ui->editroommodifybox->item(x, i)->setTextAlignment(Qt::AlignCenter);
        }
        x++;
    }
}

void MainWindow::on_btn_modRoomFloor_clicked()
{
    if (ui->cbox_roomLoc->currentText() == "") {
        ui->lbl_editUserWarning_3->setText("Please select a valid Building");
        return;
    }

    curmodifyingspace = FLOORS;
    ui->editroommodifybox->clear();
    ui->editroommodifybox->setColumnCount(1);
    ui->editroommodifybox->setRowCount(0);
    ui->editroommodifybox->horizontalHeader()->setStretchLastSection(true);
    ui->editroommodifybox->setHorizontalHeaderLabels(QStringList() << "Floor Number");
    // get building id
    QString building = ui->cbox_roomLoc->currentText();
    QSqlQuery qry = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + building);

    qry.next();
    QString buildingid = qry.value(0).toString();

    QSqlQuery result = dbManager->execQuery("SELECT FloorNo FROM Floor WHERE BuildingId=" + buildingid);

    // populate table
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        QString buildingno = result.value(0).toString();
        ui->editroommodifybox->insertRow(x);
        QStringList row;
        row << buildingno;
        for (int i = 0; i < 1; ++i)
        {
            ui->editroommodifybox->setItem(x, i, new QTableWidgetItem(row.at(i)));
            ui->editroommodifybox->item(x, i)->setTextAlignment(Qt::AlignCenter);
        }
        x++;
    }
}

void MainWindow::on_btn_modRoomRoom_clicked()
{
    if (ui->cbox_roomLoc->currentText() == "") {
        ui->lbl_editUserWarning_3->setText("Please select a valid Building");
        return;
    }

    if (ui->cbox_roomFloor->currentText() == "") {
        ui->lbl_editUserWarning_3->setText("Please select a valid Floor");
        return;
    }

    curmodifyingspace = ROOMS;
    ui->editroommodifybox->clear();
    ui->editroommodifybox->setColumnCount(1);
    ui->editroommodifybox->setRowCount(0);
    ui->editroommodifybox->horizontalHeader()->setStretchLastSection(true);
    ui->editroommodifybox->setHorizontalHeaderLabels(QStringList() << "Room Number");

    // get building id
    QString building = ui->cbox_roomLoc->currentText();
    QSqlQuery qry = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + building);

    qry.next();
    QString buildingid = qry.value(0).toString();

    // get Floor id
    QString floor = ui->cbox_roomFloor->currentText();
    QSqlQuery qry2 = dbManager->execQuery("SELECT FloorId FROM Floor WHERE BuildingId=" + buildingid + " AND FloorNo=" + floor);
    qDebug() << "SELECT FloorId FROM Floor WHERE BuildingId=" + building + " AND FloorNo=" + floor;
    qry2.next();

    QString floorid = qry2.value(0).toString();

    QSqlQuery result = dbManager->execQuery("SELECT RoomNo FROM Room WHERE FloorId=" + floorid);

    // populate table
    int x = 0;
    int qt = result.size();
    qDebug() << qt;
    while (result.next()) {
        QString buildingno = result.value(0).toString();
        ui->editroommodifybox->insertRow(x);
        QStringList row;
        row << buildingno;
        for (int i = 0; i < 1; ++i)
        {
            ui->editroommodifybox->setItem(x, i, new QTableWidgetItem(row.at(i)));
            ui->editroommodifybox->item(x, i)->setTextAlignment(Qt::AlignCenter);
        }
        x++;
    }
}

void MainWindow::on_btn_modRoomType_clicked()
{
    curmodifyingspace = TYPE;
    // this shouldn't be modifiable... - there are only ever beds and mats.
}

// HANK
void MainWindow::on_EditShiftsButton_clicked()
{
    addHistory(ADMINPAGE);
    ui->stackedWidget->setCurrentIndex(EDITSHIFT);
    ui->tableWidget_6->clearContents();

    // populate table
    // ui->tableWidget_6

    QSqlQuery shifts = dbManager->execQuery("SELECT * FROM Shift");
    while (shifts.next()) {
        int numberofshifts = shifts.value(11).toInt();
        QString day = shifts.value(0).toString();
        int dayrow = 0;
        if (day == "Monday") {
            dayrow = 0;
        } else if (day == "Tuesday") {
            dayrow = 1;
        } else if (day == "Wednesday") {
            dayrow = 2;
        } else if (day == "Thursday") {
            dayrow = 3;
        } else if (day == "Friday") {
            dayrow = 4;
        } else if (day == "Saturday") {
            dayrow = 5;
        } else if (day == "Sunday") {
            dayrow = 6;
        }

        for (int i = 1; i < (numberofshifts + 1); i++) {
            QTime starttime = shifts.value((i*2)-1).toTime();
            QTime endtime = shifts.value(i*2).toTime();

            int starthr = starttime.hour();
            int endhr = endtime.hour();

            for (int j = starthr; j <= endhr; j++) {
                QTableWidgetItem* item = new QTableWidgetItem();
                QTableWidgetItem* temp = ui->tableWidget_6->item(dayrow, j);
                QString newtxt = "";
                if (temp != 0) {
                    newtxt += temp->text();
                }
                newtxt += " " + QString::fromStdString(std::to_string(i)) + " ";
                item->setText(newtxt);
                ui->tableWidget_6->setItem(dayrow, j, item);
            }
        }
    }

    ui->comboBox_2->setCurrentIndex(1);
    ui->comboBox_2->setCurrentIndex(0);
}

void MainWindow::on_cbox_roomLoc_currentTextChanged(const QString &arg1)
{
    qDebug() << arg1;
    // clear the other boxes
    // ui->cbox_roomLoc->clear();
    ui->cbox_roomFloor->clear();
    ui->cbox_roomRoom->clear();
    ui->cbox_roomType->clear();

    if (arg1 == "") {
        // empty selected, exit function
        return;
    }

    // get building id
    QString building = ui->cbox_roomLoc->currentText();
    QSqlQuery qry = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + arg1);

    qry.next();

    // populate floor numbers based on selected building
    QSqlQuery floors = dbManager->execQuery("SELECT FloorNo FROM Floor WHERE BuildingId=" + qry.value(0).toString());

    ui->cbox_roomFloor->addItems(QStringList() << "");
    while (floors.next()) {
        ui->cbox_roomFloor->addItems(QStringList() << floors.value(0).toString());
        qDebug() << "added item" + floors.value(0).toString();
    }
}

void MainWindow::on_cbox_roomFloor_currentTextChanged(const QString &arg1)
{
    // ui->cbox_roomLoc->clear();
    // ui->cbox_roomFloor->clear();
    ui->cbox_roomRoom->clear();
    ui->cbox_roomType->clear();

    if (arg1 == "") {
        // empty selected, exit function
        return;
    }

    // get building id
    QString building = ui->cbox_roomLoc->currentText();
    QSqlQuery qry = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + building);

    qry.next();

    qDebug() << "val:" << qry.value(0).toString();

    // populate room numbers based on selected floor
    // get floor id
    QSqlQuery qry2 = dbManager->execQuery("SELECT FloorId FROM Floor WHERE BuildingId=" + qry.value(0).toString() + " AND FloorNo=" + arg1);

    qry2.next();

    QString floorid = qry2.value(0).toString();
    qDebug() << "floorid:" + floorid;

    QSqlQuery rooms = dbManager->execQuery("SELECT RoomNo FROM Room WHERE FloorId=" + floorid);

    ui->cbox_roomRoom->addItems(QStringList() << "");
    while (rooms.next()) {
        ui->cbox_roomRoom->addItems(QStringList() << rooms.value(0).toString());
    }
}

void MainWindow::on_cbox_roomRoom_currentTextChanged(const QString &arg1)
{
    // ui->cbox_roomLoc->clear();
    // ui->cbox_roomFloor->clear();
    // ui->cbox_roomRoom->clear();
    ui->cbox_roomType->clear();

    if (arg1 == "") {
        // empty selected, exit function
        return;
    }

    QSqlQuery types = dbManager->execQuery("SELECT TypeDesc FROM Type");
    while (types.next()) {
        ui->cbox_roomType->addItems(QStringList() << types.value(0).toString());
    }
}

void MainWindow::on_cbox_roomType_currentTextChanged(const QString &arg1)
{

}

void MainWindow::on_cbox_roomType_currentIndexChanged(int index)
{

}


void MainWindow::on_tableWidget_search_client_itemClicked()
{
    ui->pushButton_bookRoom->setEnabled(true);
    ui->pushButton_processPaymeent->setEnabled(true);
    ui->pushButton_editClientInfo->setEnabled(true);
    ui->pushButton_CaseFiles->setEnabled(true);
}


void MainWindow::on_programDropdown_currentIndexChanged()
{
    clearTable(ui->bookingTable);
    ui->makeBookingButton->hide();
}

void MainWindow::on_confirmAddLunch_clicked()
{
    MyCalendar* mc = new MyCalendar(this, curBook->startDate,curBook->endDate, curClient,1, curBook->room);
       mc->exec();
       delete(mc);
}

void MainWindow::on_confirmAddWake_clicked()
{
    MyCalendar* mc = new MyCalendar(this, curBook->startDate,curBook->endDate, curClient,2, curBook->room);
        mc->exec();\
        delete(mc);
}

void MainWindow::on_editLunches_clicked()
{

    ui->editUpdate->setEnabled(true);
    MyCalendar* mc;
    if(QDate::currentDate() < curBook->startDate){
        mc = new MyCalendar(this, curBook->startDate, curBook->endDate, curClient,1, curBook->room);


    }else{
       mc = new MyCalendar(this, QDate::currentDate(), curBook->endDate, curClient,1, curBook->room);
    }
       mc->exec();
       delete(mc);
}

void MainWindow::on_editWakeup_clicked()
{
    ui->editUpdate->setEnabled(true);
    MyCalendar* mc;
    if(QDate::currentDate() < curBook->startDate){

        mc = new MyCalendar(this, curBook->startDate,curBook->endDate, curClient,2, curBook->room);
    }else{

        mc = new MyCalendar(this, QDate::currentDate(),curBook->endDate, curClient,2,  curBook->room);
    }
        mc->exec();
        delete(mc);
}

void MainWindow::on_actionQuit_triggered()
{
    QApplication::quit();
}

void MainWindow::useProgressDialog(QString msg, QFuture<void> future){
    dialog->setLabelText(msg);
    futureWatcher.setFuture(future);
    dialog->setCancelButton(0);
    dialog->exec();
    futureWatcher.waitForFinished();
}

// room clicked
void MainWindow::on_tableWidget_5_clicked(const QModelIndex &index)
{
    ui->lbl_editUserWarning_3->setText("");
    // "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly"
    QString idcode = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 0)).toString();
    QString building = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 1)).toString();
    QString floor = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 2)).toString();
    QString room = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 3)).toString();
    QString bednumber = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 4)).toString();
    QString type = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 5)).toString();
    QString cost = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 6)).toString();
    QString monthly = ui->tableWidget_5->model()->data(ui->tableWidget_5->model()->index(index.row(), 7)).toString();

    // fill in stuff on the right
    populate_modRoom_cboxes();

    // set the building
    ui->cbox_roomLoc->setCurrentIndex(0);
    int currindex = 0;
    while (true) {
        if (ui->cbox_roomLoc->currentText() == building) {
            break;
        } else {
            currindex++;
            ui->cbox_roomLoc->setCurrentIndex(currindex);
        }
    }

    // set the floor
    int currindex2 = 0;
    while (true) {
        if (ui->cbox_roomFloor->currentText() == floor) {
            break;
        } else {
            currindex2++;
            ui->cbox_roomFloor->setCurrentIndex(currindex2);
        }
    }

    // set the Room
    int currindex3 = 0;
    while (true) {
        if (ui->cbox_roomRoom->currentText() == room) {
            break;
        } else {
            currindex3++;
            ui->cbox_roomRoom->setCurrentIndex(currindex3);
        }
    }

    // set the Type
    int currindex4 = 0;
    while (true) {
        if (ui->cbox_roomType->currentText().toStdString()[0] == type[0]) {
            break;
        } else {
            currindex4++;
            ui->cbox_roomType->setCurrentIndex(currindex4);
        }
    }

    // set the Space Number
    ui->le_roomNo->setText(bednumber);

    // set the prices
    // cost
    ui->doubleSpinBox->setValue(cost.toDouble());

    // monthly
    ui->doubleSpinBox_2->setValue(monthly.toDouble());
}

void MainWindow::on_lineEdit_search_clientName_returnPressed()
{
    MainWindow::on_pushButton_search_client_clicked();
}

void MainWindow::on_actionExport_to_PDF_triggered()
{
    QString rptTemplate;
    QtRPT *report = new QtRPT(this);

    // reports: daily
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == DAILYREPORT){
        rptTemplate = ":/templates/pdf/daily.xml";
        report->recordCount << checkoutReport->model.rows
                            << vacancyReport->model.rows
                            << lunchReport->model.rows
                            << wakeupReport->model.rows;
    } else

    // reports: shift
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == SHIFTREPORT){
        rptTemplate = ":/templates/pdf/shift.xml";
        report->recordCount << bookingReport->model.rows
                            << transactionReport->model.rows
                            << clientLogReport->model.rows
                            << otherReport->model.rows;
    } else

    // reports: float count
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == FLOATCOUNT){
        rptTemplate = ":/templates/pdf/float.xml";
        report->recordCount << 1;
    } else

    // reports: monthly
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == MONTHLYREPORT){
        rptTemplate = ":/templates/pdf/monthly.xml";
        report->recordCount << 1;
    } else

    // reports: restrictions
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == RESTRICTIONS){
        rptTemplate = ":/templates/pdf/restriction.xml";
        report->recordCount << yellowReport->model.rows
                            << redReport->model.rows;
    } else

    // case files pcp
    if (ui->stackedWidget->currentIndex() == CASEFILE && ui->tabw_casefiles->currentIndex() == PERSIONACASEPLAN){
        rptTemplate = ":/templates/pdf/pcp.xml";
        for (auto x: pcp_tables) {
            report->recordCount << x->rowCount();
        }
    }

    // case files pcp
    if (ui->stackedWidget->currentIndex() == CASEFILE && ui->tabw_casefiles->currentIndex() == RUNNINGNOTE){
        rptTemplate = ":/templates/pdf/running.xml";
        report->recordCount << 1;

    }

    report->loadReport(rptTemplate);

    connect(report, SIGNAL(setValue(const int, const QString, QVariant&, const int)),
           this, SLOT(setValue(const int, const QString, QVariant&, const int)));

    report->printExec();
}

void MainWindow::setValue(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage) {

    // reports: daily
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == DAILYREPORT){
        printDailyReport(recNo, paramName, paramValue, reportPage);
    } else

    // reports: shift
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == SHIFTREPORT){
        printShiftReport(recNo, paramName, paramValue, reportPage);
    } else

    // reports: float count
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == FLOATCOUNT){
        printFloatReport(recNo, paramName, paramValue, reportPage);
    } else

    // reports: monthly count
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == MONTHLYREPORT){
        printMonthlyReport(recNo, paramName, paramValue, reportPage);
    } else

    // reports: restrictions
    if (ui->stackedWidget->currentIndex() == REPORTS && ui->swdg_reports->currentIndex() == RESTRICTIONS){
        printRestrictionReport(recNo, paramName, paramValue, reportPage);
    } else

    // case files pcp
    if (ui->stackedWidget->currentIndex() == CASEFILE && ui->tabw_casefiles->currentIndex() == PERSIONACASEPLAN){
        printPCP(recNo, paramName, paramValue, reportPage);
    }

    // case files running notes
    if (ui->stackedWidget->currentIndex() == CASEFILE && ui->tabw_casefiles->currentIndex() == RUNNINGNOTE){
        printRunningNotes(recNo, paramName, paramValue, reportPage);
    }
}

void MainWindow::printDailyReport(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage){
    if (reportPage == 0){
        if (paramName == "client") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 0);
        } else if (paramName == "space") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 1);
        } else if (paramName == "start") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 2);
        } else if (paramName == "end") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 3);
        } else if (paramName == "program") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 4);
        } else if (paramName == "balance") {
            paramValue = checkoutReport->model.tableData->at(recNo * checkoutReport->model.cols + 5);
        } else if (paramName == "date") {
            paramValue = ui->dailyReport_dateEdit->text();
        } else if (paramName == "espout") {
            if (ui->lbl_espCheckouts->text().isEmpty()) return;
            paramValue = ui->lbl_espCheckouts->text();
        } else if (paramName == "espvac") {
            if (ui->lbl_espVacancies->text().isEmpty()) return;
            paramValue = ui->lbl_espVacancies->text();
        } else if (paramName == "totalout") {
            if (ui->lbl_totalCheckouts->text().isEmpty()) return;
            paramValue = ui->lbl_totalCheckouts->text();
        } else if (paramName == "totalvac") {
            if (ui->lbl_totalVacancies->text().isEmpty()) return;
            paramValue = ui->lbl_totalVacancies->text();
        }
    } else if (reportPage == 1) {
        if (paramName == "space"){
            paramValue = vacancyReport->model.tableData->at(recNo * vacancyReport->model.cols + 0);
        } else if (paramName == "prog"){
            paramValue = vacancyReport->model.tableData->at(recNo * vacancyReport->model.cols + 1);
        }
    } else if (reportPage == 2) {
        if (paramName == "client") {
            paramValue = lunchReport->model.tableData->at(recNo * lunchReport->model.cols + 0);
        } else if (paramName == "space") {
            paramValue = lunchReport->model.tableData->at(recNo * lunchReport->model.cols + 1);
        } else if (paramName == "lunches") {
            paramValue = lunchReport->model.tableData->at(recNo * lunchReport->model.cols + 2);
        }
    } else if (reportPage == 3) {
        if (paramName == "client") {
            paramValue = wakeupReport->model.tableData->at(recNo * wakeupReport->model.cols + 0);
        } else if (paramName == "space") {
            paramValue = wakeupReport->model.tableData->at(recNo * wakeupReport->model.cols + 1);
        } else if (paramName == "time") {
            paramValue = wakeupReport->model.tableData->at(recNo * wakeupReport->model.cols + 2);
        }
    }
}

void MainWindow::printShiftReport(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage){
    if (reportPage == 0){
        if (paramName == "client") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 0);
        } else if (paramName == "space") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 1);
        } else if (paramName == "program") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 2);
        } else if (paramName == "start") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 3);
        } else if (paramName == "end") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 4);
        } else if (paramName == "action") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 5);
        } else if (paramName == "staff") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 6);
        } else if (paramName == "time") {
            paramValue = bookingReport->model.tableData->at(recNo * bookingReport->model.cols + 7);
        } else if (paramName == "sname") {
            paramValue = userLoggedIn;
        } else if (paramName == "date") {
            paramValue = ui->lbl_shiftReportDateVal->text();
        } else if (paramName == "shiftNo") {
            paramValue = ui->lbl_shiftReportShiftVal->text();
        } else if (paramName == "cash") {
            paramValue = ui->lbl_cashAmt->text();
        } else if (paramName == "elec") {
            paramValue = ui->lbl_debitAmt->text();
        } else if (paramName == "depo") {
            paramValue = ui->lbl_chequeAmt->text();
        } else if (paramName == "cheque") {
            paramValue = ui->lbl_depoAmt->text();
        } else if (paramName == "total") {
            paramValue = ui->lbl_shiftAmt->text();
        }
    } else if (reportPage == 1) {
        if (paramName == "client") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 0);
        } else if (paramName == "trans") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 1);
        } else if (paramName == "type") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 2);
        } else if (paramName == "msdd") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 4);
        } else if (paramName == "cno") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 5);
        } else if (paramName == "cdate") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 6);
        } else if (paramName == "status") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 7);
        } else if (paramName == "deleted") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 8);
        } else if (paramName == "employee") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 9);
        } else if (paramName == "time") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 10);
        } else if (paramName == "amt") {
            paramValue = transactionReport->model.tableData->at(recNo * transactionReport->model.cols + 3);
        }
    } else if (reportPage == 2) {
        if (paramName == "client") {
            paramValue = clientLogReport->model.tableData->at(recNo * clientLogReport->model.cols + 0);
        } else if (paramName == "action") {
            paramValue = clientLogReport->model.tableData->at(recNo * clientLogReport->model.cols + 1);
        } else if (paramName == "employee") {
            paramValue = clientLogReport->model.tableData->at(recNo * clientLogReport->model.cols + 2);
        } else if (paramName == "time") {
            paramValue = clientLogReport->model.tableData->at(recNo * clientLogReport->model.cols + 2);
        }
    } else if (reportPage == 3) {
        if (paramName == "time") {
            paramValue = otherReport->model.tableData->at(recNo * otherReport->model.cols + 0);
        } else if (paramName == "employee") {
            paramValue = otherReport->model.tableData->at(recNo * otherReport->model.cols + 1);
        } else if (paramName == "log") {
            paramValue = otherReport->model.tableData->at(recNo * otherReport->model.cols + 2);
        }
    }
}

void MainWindow::printFloatReport(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage){
    Q_UNUSED(reportPage);
    Q_UNUSED(recNo);
    if (paramName == "nickel") {
        paramValue = ui->sbox_nickels->text();
    } else if (paramName == "dime") {
        paramValue = ui->sbox_dimes->text();
    } else if (paramName == "quarter") {
        paramValue = ui->sbox_quarters->text();
    } else if (paramName == "loonie") {
        paramValue = ui->sbox_loonies->text();
    } else if (paramName == "toonies") {
        paramValue = ui->sbox_toonies->text();
    } else if (paramName == "five") {
        paramValue = ui->sbox_fives->text();
    } else if (paramName == "ten") {
        paramValue = ui->sbox_tens->text();
    } else if (paramName == "twenty") {
        paramValue = ui->sbox_twenties->text();
    } else if (paramName == "fifty") {
        paramValue = ui->sbox_fifties->text();
    } else if (paramName == "hundred") {
        paramValue = ui->sbox_hundreds->text();
    } else if (paramName == "total") {
        paramValue = ui->lbl_floatAmt->text();
    } else if (paramName == "date") {
        paramValue = ui->cashFloatDate_lbl->text();
    } else if (paramName == "shift") {
        paramValue = ui->cashFloatShift_lbl->text();
    } else if (paramName == "comment") {
        paramValue = ui->pte_floatMemo->document()->toPlainText();
    }
}

void MainWindow::printMonthlyReport(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage){
    Q_UNUSED(reportPage);
    Q_UNUSED(recNo);
    if (paramName == "bedsUsed") {
        paramValue = ui->numBedsUsed_lbl->text();
    } else if (paramName == "bedsEmpty") {
        paramValue = ui->numEmptyBeds_lbl->text();
    } else if (paramName == "newClients") {
        paramValue = ui->numNewClients_lbl->text();
    } else if (paramName == "uniqueClients") {
        paramValue = ui->numUniqueClients_lbl->text();
    } else if (paramName == "date") {
        paramValue = ui->monthlyReportMonth_lbl->text();
    }
}


void MainWindow::printRestrictionReport(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage){
    Q_UNUSED(paramName);
    if (reportPage == 0) {
        paramValue = yellowReport->model.tableData->at(recNo * yellowReport->model.cols);
    } else if (reportPage == 1) {
        paramValue = redReport->model.tableData->at(recNo * redReport->model.cols);
    }
}

void MainWindow::printPCP(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage) {
    qDebug() << "report page: " << reportPage;

    QTableWidget *tw_pcp = pcp_tables.at(reportPage);

    if (paramName == "goal") {
        if (tw_pcp->item(recNo, 0)->text().isEmpty()) return;
         paramValue = tw_pcp->item(recNo, 0)->text();
         qDebug() << "report page: " << reportPage << " recNp: " << recNo << " value: " << tw_pcp->item(recNo, 0)->text();
    } else if (paramName == "strategy") {
        if (tw_pcp->item(recNo, 1)->text().isEmpty()) return;
         paramValue = tw_pcp->item(recNo, 1)->text();
         qDebug() << "report page: " << reportPage << " recNp: " << recNo << " value: " << tw_pcp->item(recNo, 1)->text();
    } else if (paramName == "date") {
        if (tw_pcp->item(recNo, 2)->text().isEmpty()) return;
         paramValue = tw_pcp->item(recNo, 2)->text();
         qDebug() << "report page: " << reportPage << " recNp: " << recNo << " value: " << tw_pcp->item(recNo, 2)->text();
    } else if (paramName == "person") {
        if (tw_pcp->item(recNo, 0)->text().isEmpty()) return;
         paramValue = tw_pcp->item(recNo, 0)->text();
         qDebug() << "report page: " << reportPage << " recNp: " << recNo << " value: " << tw_pcp->item(recNo, 0)->text();
    } else if (paramName == "task"){
        if (tw_pcp->item(recNo, 1)->text().isEmpty()) return;
         paramValue = tw_pcp->item(recNo, 1)->text();
         qDebug() << "report page: " << reportPage << " recNp: " << recNo << " value: " << tw_pcp->item(recNo, 1)->text();
    }
}

void MainWindow::printRunningNotes(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage) {
    Q_UNUSED(reportPage);
    Q_UNUSED(recNo);
    if (paramName == "notes") {
        if (ui->te_notes->document()->toPlainText().isEmpty()) return;
            paramValue = ui->te_notes->document()->toPlainText();
    }
}

void MainWindow::on_btn_createNewUser_3_clicked()
{
    ui->lbl_editUserWarning_3->setText("");
    // add new vacancy
    QString building = ui->cbox_roomLoc->currentText();
    QString floor = ui->cbox_roomFloor->currentText();
    QString room = ui->cbox_roomRoom->currentText();
    QString bednumber = ui->le_roomNo->text();
    QString type = ui->cbox_roomType->currentText();
    QString cost = QString::number(ui->doubleSpinBox->value());
    QString monthly = QString::number(ui->doubleSpinBox_2->value());

    // check if it already exists
    QSqlQuery result = dbManager->searchSingleBed(building, floor, room, bednumber);

    if (!result.next()) {
        // doesn't exist so make one
        // INSERT INTO Space
        // VALUES (7, 25, '', 'Mat', 0, 0, NULL);

        // get building id
        QSqlQuery bid = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + building);
        bid.next();

        // get floor id
        QSqlQuery fid = dbManager->execQuery("SELECT FloorId FROM Floor WHERE FloorNo=" + floor + " AND BuildingId=" + bid.value(0).toString());
        fid.next();

        // get room id
        QSqlQuery rid = dbManager->execQuery("SELECT RoomId FROM Room WHERE FloorId=" + fid.value(0).toString() + " AND RoomNo=" + room);
        rid.next();

        QString roomid = rid.value(0).toString();
        qDebug() << roomid;
        QSqlQuery insertres = dbManager->execQuery("INSERT INTO Space "
                                                   "VALUES (" + bednumber +
                                                   ", " + roomid +
                                                   ", '', " + "'" + type +
                                                   "', " + cost +
                                                   ", " + monthly +
                                                   ", NULL)");
        qDebug() <<"LMFAO: INSERT INTO Space "
                                     "VALUES (" + bednumber +
                                     ", " + roomid +
                                     ", '', " + "'" + type +
                                     "', " + cost +
                                     ", " + monthly +
                                     ", NULL)";
        // update spacecodes
        dbManager->updateAllSpacecodes();
        // clear everything
        ui->cbox_roomLoc->clear();
        ui->cbox_roomFloor->clear();
        ui->cbox_roomRoom->clear();
        ui->le_roomNo->clear();
        ui->cbox_roomType->clear();
        ui->le_roomNo->clear();
        ui->doubleSpinBox->setValue(0.0);
        ui->doubleSpinBox_2->setValue(0.0);
        ui->tableWidget_5->clear();
        ui->tableWidget_5->setRowCount(0);
        ui->le_users_3->clear();
        // set horizontal headers
        ui->tableWidget_5->setColumnCount(8);
        ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
        populate_modRoom_cboxes();
        ui->lbl_editUserWarning_3->setText("Vacancy created");
    } else {
        ui->lbl_editUserWarning_3->setText("This vacancy already exists. Please change the bed number.");
    }
}

void MainWindow::on_pushButton_14_clicked()
{
    ui->lbl_editUserWarning_3->setText("");

    // update vacancy
    QString building = ui->cbox_roomLoc->currentText();
    QString floor = ui->cbox_roomFloor->currentText();
    QString room = ui->cbox_roomRoom->currentText();
    QString bednumber = ui->le_roomNo->text();
    QString type = ui->cbox_roomType->currentText();
    QString cost = QString::number(ui->doubleSpinBox->value());
    QString monthly = QString::number(ui->doubleSpinBox_2->value());

    // check to make sure it exists
    QSqlQuery result = dbManager->searchSingleBed(building, floor, room, bednumber);

    if (result.next()) {
        // doesn't exist so make one
        // INSERT INTO Space
        // VALUES (7, 25, '', 'Mat', 0, 0, NULL);

        // get room id
        QSqlQuery idinfo = dbManager->searchIDInformation(building, floor, room);

        idinfo.next();

        QString roomid = idinfo.value(0).toString();

        QSqlQuery updateres = dbManager->execQuery("UPDATE s "
                                                   "SET s.cost = " + cost +
                                                   ", s.Monthly = " + monthly + " "
                                                   "FROM Space s INNER JOIN Room r ON s.RoomId = r.RoomId "
                                                   "INNER JOIN Floor f ON r.FloorId = f.FloorId "
                                                   "INNER JOIN Building b ON f.BuildingId = b.BuildingId "
                                                   "WHERE b.BuildingNo = " + building + " "
                                                   "AND f.FloorNo = " + floor + " "
                                                   "AND r.RoomNo = " + room + " "
                                                   "AND s.SpaceNo = " + bednumber);
        // update spacecodes
        dbManager->updateAllSpacecodes();

        populate_modRoom_cboxes();

        ui->lbl_editUserWarning_3->setText("Vacancy updated");
        // clear everything
        ui->cbox_roomLoc->clear();
        ui->cbox_roomFloor->clear();
        ui->cbox_roomRoom->clear();
        ui->le_roomNo->clear();
        ui->cbox_roomType->clear();
        ui->le_roomNo->clear();
        ui->doubleSpinBox->setValue(0.0);
        ui->doubleSpinBox_2->setValue(0.0);
        ui->tableWidget_5->clear();
        ui->tableWidget_5->setRowCount(0);
        ui->le_users_3->clear();
        // set horizontal headers
        ui->tableWidget_5->setColumnCount(8);
        ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
        populate_modRoom_cboxes();
    } else {
        ui->lbl_editUserWarning_3->setText("This vacancy doesn't exist. Please select a valid vacancy.");
    }
}

void MainWindow::on_editroommodifybox_clicked(const QModelIndex &index)
{
    // populate edittext on the bottom
    QString txt = ui->editroommodifybox->item(index.row(), 0)->text();
    ui->le_newTypeLoc->setText(txt);
}

void MainWindow::on_btn_newTypeLoc_clicked()
{
    QSqlQuery idinfo;

    // modify add new
    switch (curmodifyingspace) {
    case BUILDINGS: {
        QSqlQuery q1 = dbManager->execQuery("SELECT * FROM Building WHERE BuildingNo=" + ui->le_newTypeLoc->text());
        if (q1.next()) {
            ui->lbl_editUserWarning_3->setText("A building with this number already exists.");
        } else {
            QSqlQuery q2 = dbManager->execQuery("INSERT INTO Building VALUES(" + ui->le_newTypeLoc->text() + ", '')");
            ui->lbl_editUserWarning_3->setText("Building Created");

            ui->cbox_roomLoc->clear();
            ui->cbox_roomFloor->clear();
            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();
            populate_modRoom_cboxes();
            on_btn_modRoomBdlg_clicked();
        }
        break;
    }
    case FLOORS: {
//        idinfo = dbManager->execQuery("SELECT r.RoomId, b.BuildingId, f.FloorId"
//                                      "FROM Space s INNER JOIN Room r ON s.RoomId = r.RoomId INNER JOIN Floor f ON r.FloorId = f.FloorId "
//                                      "INNER JOIN Building b ON f.BuildingId = b.BuildingId "
//                                      "WHERE b.BuildingNo =" + ui->cbox_roomLoc->currentText() + " ");
//                                      "AND f.FloorNo =" + floorno + " "
//                                      "AND r.RoomNo =" + roomno + " ");
        //idinfo = dbManager->searchIDInformation(ui->cbox_roomLoc->currentText(), "1", "0");

        QSqlQuery buildingidq = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + ui->cbox_roomLoc->currentText());
        buildingidq.next();
        QSqlQuery q3 = dbManager->execQuery("SELECT * FROM Floor WHERE FloorNo=" + ui->le_newTypeLoc->text() +
                                            " AND BuildingId = " + buildingidq.value(0).toString()); // room building floor

        if (q3.next()) {
            ui->lbl_editUserWarning_3->setText("A floor with this number already exists.");
        } else {
            QSqlQuery q4 = dbManager->execQuery("INSERT INTO Floor VALUES (" + ui->le_newTypeLoc->text()
                                                + ", " + buildingidq.value(0).toString() + ")");
            ui->lbl_editUserWarning_3->setText("Floor Created");

            ui->cbox_roomFloor->clear();
            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();

            // fake an update to the building cbox to refresh the dropdown
            int before = ui->cbox_roomLoc->currentIndex();
            ui->cbox_roomLoc->setCurrentIndex(0);
            ui->cbox_roomLoc->setCurrentIndex(before);
            on_btn_modRoomFloor_clicked();
        }

        break;
    }
    case ROOMS: {
        QSqlQuery buildingidq = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + ui->cbox_roomLoc->currentText());
        buildingidq.next();
//        idinfo = dbManager->searchIDInformation(ui->cbox_roomLoc->currentText(), ui->cbox_roomFloor->currentText(), "0");
//        idinfo.next();
        QSqlQuery q3 = dbManager->execQuery("SELECT * FROM Floor WHERE FloorNo=" + ui->cbox_roomFloor->currentText() +
                                            " AND BuildingId = " + buildingidq.value(0).toString()); // room building floor
        q3.next();

        qDebug() << "Bid" + buildingidq.value(0).toString();

        QString floorid = q3.value(0).toString();

        qDebug() << "fid" + floorid;

        QSqlQuery q4 = dbManager->execQuery("SELECT * FROM Room WHERE RoomNo=" + ui->le_newTypeLoc->text()
                                            + " AND FloorId =" + floorid);

        if (q4.next()) {
            ui->lbl_editUserWarning_3->setText("A room with this number already exists.");
        } else {
            dbManager->execQuery("INSERT INTO Room VALUES (" + floorid + ", " + ui->le_newTypeLoc->text() + ")");
            ui->lbl_editUserWarning_3->setText("Room created");

            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();

            // fake an update to the floors cbox to refresh the dropdown
            int before = ui->cbox_roomFloor->currentIndex();
            ui->cbox_roomFloor->setCurrentIndex(0);
            ui->cbox_roomFloor->setCurrentIndex(before);
            on_btn_modRoomRoom_clicked();
        }

        break;
    }
    case NOT_SET: {
        ui->lbl_editUserWarning_3->setText("You haven't selected anything to modify yet");
        break;
    }
    default: {
        break;
    }
    }
    // clear everything
    ui->doubleSpinBox->setValue(0.0);
    ui->doubleSpinBox_2->setValue(0.0);
    ui->tableWidget_5->clear();
    ui->tableWidget_5->setRowCount(0);
    ui->le_users_3->clear();
    // set horizontal headers
    ui->tableWidget_5->setColumnCount(8);
    ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
    ui->le_newTypeLoc->clear();
}

void MainWindow::on_btn_delTypeLoc_clicked()
{
    QSqlQuery idinfo;

    // modify delete
    switch (curmodifyingspace) {
    case BUILDINGS: {
        QSqlQuery q1 = dbManager->execQuery("SELECT * FROM Building WHERE BuildingNo=" + ui->le_newTypeLoc->text());
        if (!q1.next()) {
            ui->lbl_editUserWarning_3->setText("No building with this number exists.");
        } else {
            QSqlQuery q2 = dbManager->execQuery("DELETE FROM Building WHERE BuildingNo=" + ui->le_newTypeLoc->text());
            ui->lbl_editUserWarning_3->setText("Building Deleted");

            ui->cbox_roomLoc->clear();
            ui->cbox_roomFloor->clear();
            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();
            populate_modRoom_cboxes();
            on_btn_modRoomBdlg_clicked();
        }
        break;
    }
    case FLOORS: {
        QSqlQuery buildingidq = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + ui->cbox_roomLoc->currentText());
        buildingidq.next();
        QSqlQuery q3 = dbManager->execQuery("SELECT * FROM Floor WHERE FloorNo=" + ui->le_newTypeLoc->text() +
                                            " AND BuildingId = " + buildingidq.value(0).toString()); // room building floor

        if (!q3.next()) {
            ui->lbl_editUserWarning_3->setText("No floor with this number exists.");
        } else {
            QSqlQuery q4 = dbManager->execQuery("DELETE FROM Floor WHERE FloorNo=" + ui->le_newTypeLoc->text()
                                                + " AND BuildingId=" + buildingidq.value(0).toString());
            ui->lbl_editUserWarning_3->setText("Floor Deleted");

            ui->cbox_roomFloor->clear();
            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();

            // fake an update to the building cbox to refresh the dropdown
            int before = ui->cbox_roomLoc->currentIndex();
            ui->cbox_roomLoc->setCurrentIndex(0);
            ui->cbox_roomLoc->setCurrentIndex(before);
            on_btn_modRoomFloor_clicked();
        }

        break;
    }
    case ROOMS: {
        QSqlQuery buildingidq = dbManager->execQuery("SELECT BuildingId FROM Building WHERE BuildingNo=" + ui->cbox_roomLoc->currentText());
        buildingidq.next();
//        idinfo = dbManager->searchIDInformation(ui->cbox_roomLoc->currentText(), ui->cbox_roomFloor->currentText(), "0");
//        idinfo.next();
        QSqlQuery q3 = dbManager->execQuery("SELECT * FROM Floor WHERE FloorNo=" + ui->cbox_roomFloor->currentText() +
                                            " AND BuildingId = " + buildingidq.value(0).toString()); // room building floor
        q3.next();


        QString floorid = q3.value(0).toString();

        qDebug() << floorid;

        QSqlQuery q4 = dbManager->execQuery("SELECT * FROM Room WHERE RoomNo=" + ui->le_newTypeLoc->text()
                                            + " AND FloorId =" + floorid);

        if (!q4.next()) {
            ui->lbl_editUserWarning_3->setText("No room with this number exists.");
        } else {
            dbManager->execQuery("DELETE FROM Room WHERE FloorId=" + floorid + " AND RoomNo=" + ui->le_newTypeLoc->text());
            ui->lbl_editUserWarning_3->setText("Room Deleted");

            ui->cbox_roomRoom->clear();
            ui->le_roomNo->clear();
            ui->cbox_roomType->clear();

            // fake an update to the floors cbox to refresh the dropdown
            int before = ui->cbox_roomFloor->currentIndex();
            ui->cbox_roomFloor->setCurrentIndex(0);
            ui->cbox_roomFloor->setCurrentIndex(before);
            on_btn_modRoomRoom_clicked();
        }
        break;
    }
    case NOT_SET: {
        break;
    }
    default: {
        break;
    }
    }
    // clear everything
    ui->le_roomNo->clear();
    ui->doubleSpinBox->setValue(0.0);
    ui->doubleSpinBox_2->setValue(0.0);
    ui->tableWidget_5->clear();
    ui->tableWidget_5->setRowCount(0);
    ui->le_users_3->clear();
    // set horizontal headers
    ui->tableWidget_5->setColumnCount(8);
    ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
    ui->le_newTypeLoc->clear();
}

void MainWindow::on_pushButton_15_clicked()
{
    ui->lbl_editUserWarning_3->setText("");
    // delete space
    QString building = ui->cbox_roomLoc->currentText();
    QString floor = ui->cbox_roomFloor->currentText();
    QString room = ui->cbox_roomRoom->currentText();
    QString bednumber = ui->le_roomNo->text();
    QString type = ui->cbox_roomType->currentText();
    QString cost = QString::number(ui->doubleSpinBox->value());
    QString monthly = QString::number(ui->doubleSpinBox_2->value());

    // check to make sure it exists
    // check if it already exists
    QSqlQuery result = dbManager->searchSingleBed(building, floor, room, bednumber);

    if (result.next()) {
        // doesn't exist so make one
        QSqlQuery deleteres = dbManager->deleteSpace(building, floor, room, bednumber);

        populate_modRoom_cboxes();

        ui->lbl_editUserWarning_3->setText("Vacancy Deleted");

        // clear everything
        ui->cbox_roomLoc->clear();
        ui->cbox_roomFloor->clear();
        ui->cbox_roomRoom->clear();
        ui->le_roomNo->clear();
        ui->cbox_roomType->clear();
        ui->le_roomNo->clear();
        ui->doubleSpinBox->setValue(0.0);
        ui->doubleSpinBox_2->setValue(0.0);
        ui->tableWidget_5->clear();
        ui->tableWidget_5->setRowCount(0);
        ui->le_users_3->clear();
        // set horizontal headers
        ui->tableWidget_5->setColumnCount(8);
        ui->tableWidget_5->setHorizontalHeaderLabels(QStringList() << "ID Code" << "Building" << "Floor" << "Room" << "Bed Number" << "Type" << "Cost" << "Monthly");
        populate_modRoom_cboxes();

    } else {
        ui->lbl_editUserWarning_3->setText("This vacancy doesn't exist. Please select a valid vacancy.");
    }
}

void MainWindow::on_chk_filter_clicked()
{
    if (!ui->chk_filter->isChecked()){
        QStringList filter;
        populate_tw_caseFiles(filter);
    } else {
        int nRow = ui->tableWidget_search_client->currentRow();

        QStringList filter = (QStringList() << "*" + ui->tableWidget_search_client->item(nRow, 3)->text() + ", " +
                              ui->tableWidget_search_client->item(nRow, 1)->text() + "*");
        populate_tw_caseFiles(filter);
    }
}

void MainWindow::on_btn_payNew_clicked()
{
    ui->stackedWidget->setCurrentIndex(CLIENTLOOKUP);
    ui->pushButton_processPaymeent->setHidden(false);
}

void MainWindow::on_actionLogout_triggered()
{
    LoginPrompt* w = new LoginPrompt();

    w->show();

    // kill the current thread
    work->cont = false;

    close();
}



void MainWindow::on_editProgramDrop_currentIndexChanged(const QString &arg1)
{
    ui->editUpdate->setEnabled(true);
}

void MainWindow::on_comboBox_3_currentTextChanged(const QString &arg1)
{
    if (arg1 == "1") {
        ui->comboBox_4->clear();
        ui->comboBox_4->addItem("1");
    } else if (arg1 == "2") {
        ui->comboBox_4->clear();
        ui->comboBox_4->addItem("1");
        ui->comboBox_4->addItem("2");
    } else if (arg1 == "3") {
        ui->comboBox_4->clear();
        ui->comboBox_4->addItem("1");
        ui->comboBox_4->addItem("2");
        ui->comboBox_4->addItem("3");
    } else if (arg1 == "4") {
        ui->comboBox_4->clear();
        ui->comboBox_4->addItem("1");
        ui->comboBox_4->addItem("2");
        ui->comboBox_4->addItem("3");
        ui->comboBox_4->addItem("4");
    } else if (arg1 == "5"){
        ui->comboBox_4->clear();
        ui->comboBox_4->addItem("1");
        ui->comboBox_4->addItem("2");
        ui->comboBox_4->addItem("3");
        ui->comboBox_4->addItem("4");
        ui->comboBox_4->addItem("5");
    }

    qDebug() << currentshiftid;
}

void MainWindow::setShift() {

   QString connName = "leshiftname";
   {
       QSqlDatabase tempDb = QSqlDatabase::database();
       if (dbManager->createDatabase(&tempDb, connName))
       {
           QSqlQuery query(tempDb);

           qDebug() << "Setting shift now";
           currentshiftid = -1;
           // current time
           QDateTime curtime = QDateTime::currentDateTime();
           QTime curactualtime = QTime::currentTime();
           int dayofweek = curtime.date().dayOfWeek();
           //qDebug() << "today is: " + dayofweek;

           if (dayofweek == 1) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Monday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 2) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Tuesday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 3) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Wednesday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 4) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Thursday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 5) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Friday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 6) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Saturday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           } else if (dayofweek == 7) {
               query.exec("SELECT * FROM Shift WHERE DayOfWeek='Sunday'");

               query.next();
               int numberofshifts = query.value(11).toInt();

               for (int i = 1; i < (numberofshifts + 1); i++) {
                   QTime starttime = query.value((i*2)-1).toTime();
                   QTime endtime = query.value(i*2).toTime();

                   if ((curactualtime < endtime) && (curactualtime >= starttime)) {
                       // change the shiftnum
                       currentshiftid = i;
                       break;
                   }
               }
           }

//               QSqlQuery query(tempDb);
//               if (!query.exec("SELECT ProfilePic FROM Client WHERE ClientId =" + ClientId))
//               {
//                   qDebug() << "downloadProfilePic query failed";
//                   return false;
//               }
//               query.next();
//               QByteArray data = query.value(0).toByteArray();
//               *img = QImage::fromData(data, "PNG");

       }
       tempDb.close();
   } // Necessary braces: tempDb and query are destroyed because out of scope
   QSqlDatabase::removeDatabase(connName);

//    statusBar()->removeWidget(lbl_curShift);
//    statusBar()->removeWidget(lbl_curUser);
//    statusBar()->addPermanentWidget(lbl_curUser);
//    lbl_curShift = new QLabel();
//    lbl_curShift->setText("Shift Number: " + currentshiftid);
//    statusBar()->addPermanentWidget(lbl_curShift);
    qDebug() << "Updated Shift";
}

void MainWindow::on_btn_saveShift_clicked()
{
    QString day = ui->comboBox_2->currentText();
    QString shiftno = ui->comboBox_4->currentText();

    QString starttime = ui->timeEdit->text();
    QString endtime = ui->timeEdit_2->text();

    // if the shift does not exist, make one
    QSqlQuery existcheck = dbManager->execQuery("SELECT * FROM Shift WHERE DayOfWeek='" + day + "'");

    if (!existcheck.next()) {
        qDebug() << "Doesn't exist";
        // insert
        QSqlQuery insert = dbManager->execQuery("INSERT INTO Shift VALUES('" + day + "'"
                                                ", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, " + ui->comboBox_3->currentText() + ")");
    }

    // update
    QSqlQuery update = dbManager->execQuery("UPDATE Shift SET StartTimeShift" + shiftno +
                                            "='" + starttime + "' WHERE DayOfWeek = '" + day + "'");
    QSqlQuery update2 = dbManager->execQuery("UPDATE Shift SET EndTimeShift" + shiftno +
                                            "='" + endtime + "' WHERE DayOfWeek = '" + day + "'");
    dbManager->execQuery("UPDATE Shift SET NumShifts=" + ui->comboBox_3->currentText() + " WHERE DayOfWeek = '" + day + "'");

    on_EditShiftsButton_clicked();
}

void MainWindow::on_comboBox_2_currentTextChanged(const QString &arg1)
{
    QSqlQuery existcheck = dbManager->execQuery("SELECT * FROM Shift WHERE DayOfWeek='" + arg1 + "'");

    if (existcheck.next()) {
        int numshifts = existcheck.value(11).toInt();
        ui->comboBox_3->setCurrentIndex(numshifts-1);
    }
}

void MainWindow::updatemenuforuser() {
    QSqlQuery roleq = dbManager->execQuery("SELECT Role FROM Employee WHERE Username='" + userLoggedIn + "'");

    roleq.next();

    qDebug() << "ROLE: " << roleq.value(0).toString();
    qDebug() << "USER: " << userLoggedIn;

    if (roleq.value(0).toString() == "STANDARD") {
        QSizePolicy sp_retain = ui->caseButton->sizePolicy();
        sp_retain.setRetainSizeWhenHidden(true);
        ui->caseButton->setSizePolicy(sp_retain);
        ui->caseButton->hide();

        QSizePolicy sp_retain2 = ui->caseButton->sizePolicy();
        sp_retain2.setRetainSizeWhenHidden(true);
        ui->adminButton->setSizePolicy(sp_retain2);
        ui->adminButton->hide();
    } else if (roleq.value(0).toString() == "CASE WORKER") {
        QSizePolicy sp_retain = ui->caseButton->sizePolicy();
        sp_retain.setRetainSizeWhenHidden(true);
        ui->adminButton->setSizePolicy(sp_retain);
        ui->adminButton->hide();
    }

    //display logged in user and current shift in status bar
    lbl_curUser = new QLabel("Logged in as: " + userLoggedIn + "  ");
    // lbl_curShift = new QLabel("Shift Number: " + currentshiftid);
    statusBar()->addPermanentWidget(lbl_curUser);
    // statusBar()->addPermanentWidget(lbl_curShift);
}

void MainWindow::on_editRemoveCheque_clicked()
{
    int row = ui->mpTable->selectionModel()->currentIndex().row();
    if(row == -1)
        return;
    QString chequeNo = "";
    QString transId = ui->mpTable->item(row, 6)->text();
    double retAmt = ui->mpTable->item(row, 3)->text().toDouble();
    QString clientId = ui->mpTable->item(row, 5)->text();
    curClient = new Client();
    popClientFromId(clientId);
    double curBal = curClient->balance + retAmt;
    if(!dbManager->setPaid(transId, chequeNo )){

    }
    ui->mpTable->removeRow(row);
}


void MainWindow::on_storage_clicked()
{
    ui->stackedWidget->setCurrentIndex(STORAGEPAGE);
}

void MainWindow::on_storesearch_clicked()
{
    QSqlQuery result;
    result = dbManager->getFullStorage();
    QStringList header, cols;
    header << "Name" << "Date" << "Items" << "" << "";
    cols   << "StorageUserName" << "StorageDate" << "StorageItems" << "ClientId" << "StorageId";
    populateATable(ui->storageTable,header,cols, result, true);
    ui->storageTable->setColumnHidden(3, true);
    ui->storageTable->setColumnHidden(4, true);

}

void MainWindow::on_confirmStorage_clicked()
{
    Storage * store = new Storage(this, curClient->clientId, curClient->fullName, "", "");
    store->exec();


    delete(store);
}

void MainWindow::on_storageEdit_clicked()
{
    int row = ui->storageTable->selectionModel()->currentIndex().row();
    if(row == -1)
        return;
    QString client, storeId, items, name;
    client = ui->storageTable->item(row, 3)->text();
    storeId = ui->storageTable->item(row, 4)->text();
    items = ui->storageTable->item(row, 2)->text();
    name = ui->storageTable->item(row, 0)->text();
    Storage * store = new Storage(this, client, name, items, storeId);
    store->exec();
    delete(store);
    on_storesearch_clicked();
}

void MainWindow::on_storageTable_itemClicked(QTableWidgetItem *item)
{
    int row = item->row();
    QString items = ui->storageTable->item(row, 2)->text();
    ui->storageInfo->clear();
    ui->storageInfo->setEnabled(false);
    ui->storageInfo->insertPlainText(items);
}

void MainWindow::on_storageDelete_clicked()
{
    int row = ui->storageTable->selectionModel()->currentIndex().row();
    if(row == -1)
        return;
    if(!doMessageBox("Are you sure you want to delete?"))
        return;
    QString storeId;
    storeId = ui->storageTable->item(row, 4)->text();
    if(!dbManager->removeStorage(storeId))
        qDebug() << "Error removing storage";
}
