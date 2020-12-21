#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include <vtkDataSetReader.h>

#include "vtkBoundedPlanePointPlacer.h"
#include "vtkCommand.h"
#include "vtkDICOMImageReader.h"
#include "vtkDistanceRepresentation.h"
#include "vtkDistanceRepresentation2D.h"
#include "vtkDistanceWidget.h"
#include "vtkHandleRepresentation.h"
#include "vtkImageData.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkImageSlabReslice.h"
#include "vtkInteractorStyleImage.h"
#include "vtkLookupTable.h"
#include "vtkPlane.h"
#include "vtkPlaneSource.h"
#include "vtkPointHandleRepresentation2D.h"
#include "vtkPointHandleRepresentation3D.h"
#include "vtkProperty.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkResliceCursor.h"
#include "vtkResliceCursorActor.h"
#include "vtkResliceCursorLineRepresentation.h"
#include "vtkResliceCursorPolyDataAlgorithm.h"
#include "vtkResliceCursorThickLineRepresentation.h"
#include "vtkResliceCursorWidget.h"
#include "vtkResliceImageViewer.h"
#include "vtkResliceImageViewerMeasurements.h"
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

class vtkResliceCursorCallback : public vtkCommand
{
public:
    static vtkResliceCursorCallback* New() { return new vtkResliceCursorCallback; }

    void Execute(vtkObject* caller, unsigned long ev, void* callData) override
    {

        if (ev == vtkResliceCursorWidget::WindowLevelEvent || ev == vtkCommand::WindowLevelEvent ||
            ev == vtkResliceCursorWidget::ResliceThicknessChangedEvent)
        {
            // Render everything
            for (int i = 0; i < 3; i++)
            {
                this->RCW[i]->Render();
            }
            this->IPW[0]->GetInteractor()->GetRenderWindow()->Render();
            return;
        }

        vtkImagePlaneWidget* ipw = dynamic_cast<vtkImagePlaneWidget*>(caller);
        if (ipw)
        {
            double* wl = static_cast<double*>(callData);

            if (ipw == this->IPW[0])
            {
                this->IPW[1]->SetWindowLevel(wl[0], wl[1], 1);
                this->IPW[2]->SetWindowLevel(wl[0], wl[1], 1);
            }
            else if (ipw == this->IPW[1])
            {
                this->IPW[0]->SetWindowLevel(wl[0], wl[1], 1);
                this->IPW[2]->SetWindowLevel(wl[0], wl[1], 1);
            }
            else if (ipw == this->IPW[2])
            {
                this->IPW[0]->SetWindowLevel(wl[0], wl[1], 1);
                this->IPW[1]->SetWindowLevel(wl[0], wl[1], 1);
            }
        }

        vtkResliceCursorWidget* rcw = dynamic_cast<vtkResliceCursorWidget*>(caller);
        if (rcw)
        {
            vtkResliceCursorLineRepresentation* rep = dynamic_cast<vtkResliceCursorLineRepresentation*>(rcw->GetRepresentation());
            // Although the return value is not used, we keep the get calls
            // in case they had side-effects
            rep->GetResliceCursorActor()->GetCursorAlgorithm()->GetResliceCursor();
            for (int i = 0; i < 3; i++)
            {
                vtkPlaneSource* ps = static_cast<vtkPlaneSource*>(this->IPW[i]->GetPolyDataAlgorithm());
                ps->SetOrigin(this->RCW[i]->GetResliceCursorRepresentation()->GetPlaneSource()->GetOrigin());
                ps->SetPoint1(this->RCW[i]->GetResliceCursorRepresentation()->GetPlaneSource()->GetPoint1());
                ps->SetPoint2(this->RCW[i]->GetResliceCursorRepresentation()->GetPlaneSource()->GetPoint2());

                // If the reslice plane has modified, update it on the 3D widget
                this->IPW[i]->UpdatePlacement();
            }
        }

        // Render everything
        for (int i = 0; i < 3; i++)
        {
            this->RCW[i]->Render();
        }
        this->IPW[0]->GetInteractor()->GetRenderWindow()->Render();
    }

    vtkResliceCursorCallback() {}
    vtkImagePlaneWidget* IPW[3];
    vtkResliceCursorWidget* RCW[3];
};

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::AboutDialog()
{
    QMessageBox::information(this, "About","By Martijn Koopman.\nSource code available under Apache License 2.0.");
}

void MainWindow::Render()
{
    for (int i = 0; i < 3; i++)
    {
        riw[i]->Render();
    }
    ui->view3->renderWindow()->Render();
    ui->view2->renderWindow()->Render();
}

void MainWindow::OpenVTK()
{
    QString fileVTK = QFileDialog::getOpenFileName(this, tr("Open file"), "", "VTK Files (*.vtk)");
    
    QFile file(fileVTK);
    file.open(QIODevice::ReadOnly);
    if (!file.exists())
        return;

    readVTK(fileVTK);
}

void MainWindow::OpenDICOM()
{
    QString folderDICOM = QFileDialog::getExistingDirectory(this, tr("Open DCM Folder"), QDir::currentPath(), QFileDialog::ShowDirsOnly);
   
    QFile file(folderDICOM);
    file.open(QIODevice::ReadOnly);
    if (!file.exists())
        return;

    readDICOM(folderDICOM);
}

void MainWindow::readDICOM(const QString& folderDICOM)
{
    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetDirectoryName(folderDICOM.toStdString().c_str());
    reader->Update();
    int imageDims[3];
    reader->GetOutput()->GetDimensions(imageDims);

    for (int i = 0; i < 3; i++)
    {
        riw[i] = vtkSmartPointer<vtkResliceImageViewer>::New();
        vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
        riw[i]->SetRenderWindow(renderWindow);
    }
    ui->view1->setRenderWindow(riw[0]->GetRenderWindow());
    riw[0]->SetupInteractor(ui->view1->renderWindow()->GetInteractor());

    ui->view2->setRenderWindow(riw[1]->GetRenderWindow());
    riw[1]->SetupInteractor(ui->view2->renderWindow()->GetInteractor());

    ui->view3->setRenderWindow(riw[2]->GetRenderWindow());
    riw[2]->SetupInteractor(ui->view3->renderWindow()->GetInteractor());

    for (int i = 0; i < 3; i++)
    {
        // make them all share the same reslice cursor object.
        vtkResliceCursorLineRepresentation* rep = vtkResliceCursorLineRepresentation::SafeDownCast(
            riw[i]->GetResliceCursorWidget()->GetRepresentation());
        riw[i]->SetResliceCursor(riw[0]->GetResliceCursor());

        rep->GetResliceCursorActor()->GetCursorAlgorithm()->SetReslicePlaneNormal(i);

        riw[i]->SetInputData(reader->GetOutput());
        riw[i]->SetSliceOrientation(i);
        riw[i]->SetResliceModeToAxisAligned();
    }
}

void MainWindow::readVTK(const QString& fileVTK)
{
    ui->sceneWidget->removeDataSet();

    // Create reader
    vtkSmartPointer<vtkDataSetReader> reader = vtkSmartPointer<vtkDataSetReader>::New();
    reader->SetFileName(fileVTK.toStdString().c_str());

    // Read the file
    reader->Update();

    // Add data set to 3D view
    vtkSmartPointer<vtkDataSet> dataSet = reader->GetOutput();
    if (dataSet != nullptr) {
        ui->sceneWidget->addDataSet(reader->GetOutput());
    }
}
