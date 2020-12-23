#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "vtkImagePlaneWidget.h"
#include "vtkResliceImageViewer.h"
#include "vtkResliceImageViewerMeasurements.h"
#include "vtkSmartPointer.h"

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow 
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

public slots:
    void AboutDialog();
    void Render();
    void OpenVTK();
    void OpenDICOM();

//public Q_SLOTS:
  //  virtual void Render();

protected:
    void readDICOM(const QString&);
    void readVTK(const QString&);
    vtkSmartPointer<vtkResliceImageViewer> riw[3];

private:
    Ui::MainWindow* ui;
};

#endif // MAINWINDOW_H
