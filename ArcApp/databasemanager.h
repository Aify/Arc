#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QtSql/QtSql>
#include <stdio.h>
#include <cstdarg>
#include <iostream>
#include <fstream>
#include "shared.h"
#include "dbconfig.h"
class DatabaseManager
{
public:
    DatabaseManager();
    void print();
    QSqlQuery selectAll(QString tableName);
    QSqlQuery loginSelect(QString username, QString password);
    QSqlQuery findUser(QString username);
    QSqlQuery addNewEmployee(QString username, QString password, QString role);
    void printAll(QSqlQuery queryResults);
    QSqlQuery getCurrentBooking(QDate start, QDate end, QString program);
    QSqlQuery getPrograms();
    bool insertBookingTable(QString insert);
    int getMonthlyRate(QString room, QString program);
    QSqlQuery getLatestFileUploadEntry(QString tableName);
    bool uploadCaseFile(QString filepath);
    QSqlQuery execQuery(QString queryString);
    bool addPayment(QString values);
    QSqlQuery getActiveBooking(QString user, bool userLook);
    bool downloadLatestCaseFile();

private:
    QSqlDatabase db;
};

extern DatabaseManager* dbManager;

#endif // DATABASEMANAGER_H
