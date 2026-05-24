#pragma once

#include <QMainWindow>

class GameWidget;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    GameWidget* gameWidget_;
};
