#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "vtkImagePlaneWidget.h"
#include "vtkResliceImageViewer.h"
#include "vtkResliceImageViewerMeasurements.h"
#include "vtkSmartPointer.h"
#include <vtkDICOMImageReader.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>

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
    void showAboutDialog();
    void Render();
    void OpenVTK();
    void OpenDICOM();
    void refresh();
    void reading();
    // void setValue();

private slots:
    // define fucntion receive value
    void horizontal_level(int);
    void vertical_window(int);
    //void on_pushButton_clicked();

protected:
   void readDICOM(const QString&);
   void readVTK(const QString&);
   vtkSmartPointer<vtkResliceImageViewer> riw[3];
   vtkSmartPointer<vtkDICOMImageReader> reader_data = vtkSmartPointer<vtkDICOMImageReader>::New();
   vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
   vtkRenderer* renderer;

private:
    Ui::MainWindow* ui;
    int dim[3];
};

#endif // MAINWINDOW_H
