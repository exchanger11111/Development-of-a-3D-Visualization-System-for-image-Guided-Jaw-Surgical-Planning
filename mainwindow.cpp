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
#include <vtkImageReslice.h>
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
#include <vtkDICOMReader.h>
#include <vtkImageActor.h>
#include <vtkDICOMDirectory.h>
#include <vtkDICOMItem.h>
#include <vtkStringArray.h>
#include <vtkDICOMDictionary.h>
#include <vtkDICOMCTRectifier.h>
#include <vtkDICOMApplyRescale.h>
#include<vtkImageShiftScale.h>
#include <vtkImageSliceMapper.h>


//This class is used for syncing four pane 
class vtkResliceCursorCallback : public vtkCommand
{
public:
    //responsible from syncing four pane 
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
                this->IPW[2]->SetWindowLevel(wl[0], wl[1], 1);
                this->IPW[1]->SetWindowLevel(wl[0], wl[1], 1);
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
    QMessageBox::information(this, "About","Modified By Gan Hui Wen.\nThis is an application for 3D Visualization System");
}

void MainWindow::Render()
{
    for (int i = 0; i < 3; i++)
    {
        riw[i]->Render();
    }
    ui->view3->renderWindow()->Render();
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
void MainWindow::refresh()
{
    vtkSmartPointer<vtkImageShiftScale> shiftScaleFilter = vtkSmartPointer<vtkImageShiftScale>::New();
    shiftScaleFilter->SetOutputScalarTypeToUnsignedChar();
    
    shiftScaleFilter->SetShift(0);
    shiftScaleFilter->SetScale(0);
    shiftScaleFilter->Update();

    // Create actors
    vtkSmartPointer<vtkImageSliceMapper> originalSliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();

    vtkSmartPointer<vtkImageSlice> originalSlice = vtkSmartPointer<vtkImageSlice>::New();
    originalSlice->SetMapper(originalSliceMapper);

    vtkSmartPointer<vtkImageSliceMapper> shiftScaleMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
    shiftScaleMapper->SetInputConnection(shiftScaleFilter->GetOutputPort());

    vtkSmartPointer<vtkImageSlice> shiftScaleSlice = vtkSmartPointer<vtkImageSlice>::New();
    shiftScaleSlice->SetMapper(shiftScaleMapper);
   // shiftScaleSlice->GetProperty()->SetInterpolationTypeToNearest();

    //renderer->AddViewProp(originalSlice);

    vtkSmartPointer<vtkRenderer> shiftScaleRenderer = vtkSmartPointer<vtkRenderer>::New();
    shiftScaleRenderer->AddViewProp(shiftScaleSlice);

    ui->view1->update();
    ui->view2->update();
    ui->view3->update();
}
int MainWindow::setValue(int v)
{
    if (v > 0)
    {
        
    }
    return 0;
}

void MainWindow::readDICOM(const QString& folderDICOM)
{
    //vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    //reader->SetDirectoryName(folderDICOM.toStdString().c_str());
    //reader->Update();

    vtkSmartPointer < vtkDICOMDirectory > dicomdir = vtkSmartPointer < vtkDICOMDirectory >::New();
    dicomdir->SetDirectoryName(folderDICOM.toStdString().c_str());
    dicomdir->RequirePixelDataOn();
    dicomdir->Update();
    int n = dicomdir->GetNumberOfSeries();

    vtkSmartPointer <vtkDICOMReader> reader =  vtkSmartPointer <vtkDICOMReader>::New();

    // Provide a multi-frame, multi-stack file
    //reader->SetFileName(folderDICOM.toStdString().c_str());
    // Read the meta data, get a list of stacks
   // reader->UpdateInformation();
    vtkStringArray* stackNames = reader->GetStackIDs();
    // Specify a stack, here we assume we know the name:
    reader->SetDesiredStackID("1");

    
    if (n > 0)
    {
        // read the first series found
        reader->SetFileNames(dicomdir->GetFileNamesForSeries(0));
        reader->Update();
    }
    else
    {
        //std::cerr << "No DICOM images in directory!" << std::endl;
        QMessageBox::information(this, "About", " No DICOM images in directory!");
        return;
    }
    
    reader->SetMemoryRowOrderToFileNative();
    reader->Update();
    // get the matrix to use when displaying the data
    // (this matrix provides position and orientation)
   // vtkMatrix4x4* matrix = reader->GetPatientMatrix();
    
    vtkNew<vtkDICOMCTRectifier> rectify;
    rectify->SetVolumeMatrix(reader->GetPatientMatrix());
    rectify->SetInputConnection(reader->GetOutputPort());
    rectify->Update();

    // get the new PatientMatrix for the rectified volume
    vtkMatrix4x4* matrix = rectify->GetRectifiedMatrix();

    vtkSmartPointer <vtkImageActor> actor = vtkSmartPointer <vtkImageActor>::New();
    actor->GetMapper();
         // SetInputConnection(reader->GetOutputPort());
    actor->SetUserMatrix(matrix);


   // Flip the order of dicom pic to become ascending order
    vtkSmartPointer<vtkImageReslice> flip = vtkSmartPointer<vtkImageReslice>::New();
    //flip->SetInputConnection(reader->GetOutputPort());
    //flip->SetResliceAxesDirectionCosines(-1, 0, 0, 0, -1, 0, 0, 0, -1);
    //flip->Update();

    int imageDims[3];
    reader->GetOutput()->GetDimensions(imageDims);

    for (int i = 0; i < 3; i++)
    {
        riw[i] = vtkSmartPointer<vtkResliceImageViewer>::New();        
        vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
        riw[i]->SetRenderWindow(renderWindow);
    }
    //link QT ui to vtk pane
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

void MainWindow::OpenVTK()
{
    QString fileVTK = QFileDialog::getOpenFileName(this, tr("Open file"), "", "VTK Files (*.vtk)");
    
    QFile file(fileVTK);
    file.open(QIODevice::ReadOnly);
    if (!file.exists())
        return;

    readVTK(fileVTK);
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
    if (dataSet != nullptr) 
    {
        ui->sceneWidget->addDataSet(reader->GetOutput());
    }
}
