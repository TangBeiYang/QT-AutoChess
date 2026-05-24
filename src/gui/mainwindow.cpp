#include "mainwindow.h"

#include "gamewidget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      gameWidget_(new GameWidget(this)) {
    setWindowTitle("Auto Chess Board");
    resize(980, 900);
    setCentralWidget(gameWidget_);
}
