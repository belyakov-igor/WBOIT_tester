#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void InitWalls() const;
    void UpdateWalls(float p) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // MAINWINDOW_H
