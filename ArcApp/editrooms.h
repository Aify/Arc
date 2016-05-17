#ifndef EDITROOMS_H
#define EDITROOMS_H

#include <QDialog>
#include "booking.h"
#include "databasemanager.h"
#include <QTableWidgetItem>
#include "swapper.h"
#include "client.h"
namespace Ui {
class EditRooms;
}

class EditRooms : public QDialog
{
    Q_OBJECT

public:
    explicit EditRooms(QWidget *parent = 0);
    ~EditRooms();
    EditRooms(QWidget *parent, Booking * curBook, QString empId, QString shift, Client *  cli);
    void searchAvailable(QString program);
    void populateATable(QTableWidget * table, QStringList headers, QStringList items, QSqlQuery result, bool stretch);
    void getProgramCodes(QString cur);
    bool setup;
    void hideSwap();
    void showSwap();
    void popSwapClient(int row);
    void displaySwapClient();
    void calcCosts();
    bool checkNumber(QString num);
    void setCurbook();
private slots:
    void on_editProgram_currentIndexChanged(const QString &arg1);

    void on_buttonBox_accepted();

    void on_editSwap_clicked();

    void on_editRoom_itemSelectionChanged();

private:
    bool swapping;
    bool addQuery;
    Booking * curBook;
    Booking * swapBook;
    Client * curClient;
    Client * swapClient;
    Ui::EditRooms *ui;
    QString empId;
    QString shift;
    QStringList fastQuery;
};

#endif // EDITROOMS_H
