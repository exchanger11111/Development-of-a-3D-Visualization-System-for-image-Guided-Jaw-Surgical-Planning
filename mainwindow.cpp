#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include <vtkDataSetReader.h>

#include "vtkBoundedPlanePointPlacer.h"
#include "vtkCommand.h"
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
#include <vtkRenderWindow.h>

#include <vtkImageActor.h>
#include <vtkDICOMDirectory.h>
#include <vtkDICOMItem.h>
#include <vtkStringArray.h>
#include <vtkDICOMDictionary.h>
#include <vtkDICOMCTRectifier.h>
#include <vtkDICOMApplyRescale.h>
#include<vtkImageShiftScale.h>
#include <vtkImageSliceMapper.h>
#include <vtkDICOMMetaData.h>
#include <vtkDICOMApplyPalette.h>
#include <vtkCamera.h>
#include <vtkImageCast.h>
#include <vtkDICOMReader.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageActor.h>
#include <vtkMatrix4x4.h>
#include <vtkImageFlip.h>

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

void MainWindow::showAboutDialog()
{
    QMessageBox::information(this, "About", "Modified By Gan Hui Wen.\nThis is an application for 3D Visualization System");
}

void MainWindow::Render()
{
    for (int i = 0; i < 3; i++)
    {
        riw[i]->Render();
    }
    ui->view3->renderWindow()->Render();
}

void MainWindow::reading()
{
    vtkImageData* readerImg = nullptr;
    readerImg = this->reader_data->GetOutput();
    readerImg->GetDimensions(this->dim);

    if (dim[0] > 2 || dim[1] > 2 || dim[2] > 2)
    {
        QString dim0 = QString::number(dim[0]);
        QString dim1 = QString::number(dim[1]);
        QString dim2 = QString::number(dim[2]);
        QString space = " ";

        ui->textBrowser->append("\n");

        ui->textBrowser->append("Dimensions:" + space + dim0 + space + dim1 + space + dim2);
        QString fileExtensions = this->reader_data->GetFileExtensions();
        ui->textBrowser->append("fileExtensions: " + fileExtensions);

        QString descriptiveName = this->reader_data->GetDescriptiveName();
        ui->textBrowser->append("descriptiveName: " + descriptiveName);

        double* pixelSpacing = this->reader_data->GetPixelSpacing();
        QString pixelS = QString::number(*pixelSpacing, 10, 5);
        ui->textBrowser->append("pixelSpacing: " + pixelS);

        int width = this->reader_data->GetWidth();
        QString wid = QString::number(width);
        ui->textBrowser->append("width: " + wid);

        int height = this->reader_data->GetHeight();
        QString heig = QString::number(height);
        ui->textBrowser->append("height: " + heig);

        float* imagePositionPatient = this->reader_data->GetImagePositionPatient();
        QString imPP = QString::number(*imagePositionPatient, 10, 5);
        ui->textBrowser->append("imagePositionPatient: " + imPP);

        float* imageOrientationPatient = this->reader_data->GetImageOrientationPatient();
        QString imOP = QString::number(*imageOrientationPatient, 10, 5);
        ui->textBrowser->append("imageOrientationPatient: " + imOP);

        int bitsAllocated = this->reader_data->GetBitsAllocated();
        QString bitsA = QString::number(bitsAllocated);
        ui->textBrowser->append("bitsAllocated: " + bitsA);

        int pixelRepresentation = this->reader_data->GetPixelRepresentation();
        QString pixelR = QString::number(pixelRepresentation);
        ui->textBrowser->append("pixelRepresentation: " + pixelR);

        int numberOfComponents = this->reader_data->GetNumberOfComponents();
        QString numberO = QString::number(numberOfComponents);
        ui->textBrowser->append("numberOfComponents: " + numberO);

        QString transferSyntaxUID = this->reader_data->GetTransferSyntaxUID();
        ui->textBrowser->append("transferSyntaxUID: " + transferSyntaxUID);

        float rescaleSlope = this->reader_data->GetRescaleSlope();
        QString rescaleS = QString::number(rescaleSlope, 10, 5);
        ui->textBrowser->append("rescaleSlope: " + rescaleS);

        float rescaleOffset = this->reader_data->GetRescaleOffset();
        QString rescaleO = QString::number(rescaleOffset, 10, 5);
        ui->textBrowser->append("rescaleOffset: " + rescaleO);

        QString patientName = this->reader_data->GetPatientName();
        ui->textBrowser->append("patientName: " + patientName);

        QString studyUID = this->reader_data->GetStudyUID();
        ui->textBrowser->append("studyUID: " + studyUID);

        QString studyID = this->reader_data->GetStudyID();
        ui->textBrowser->append("studyID: " + studyID);

        float gantryAngle = this->reader_data->GetGantryAngle();
        QString gantryA = QString::number(gantryAngle, 10, 5);
        ui->textBrowser->append("gantryAngle: " + gantryA);
    }
}

void MainWindow::OpenDICOM()
{
    QString folderDICOM = QFileDialog::getExistingDirectory(this, tr("Open DCM Folder"), QDir::currentPath(), QFileDialog::ShowDirsOnly);

    QFile file(folderDICOM);
    file.open(QIODevice::ReadOnly);
    if (!file.exists())
        return;

    ui->spinBox->setRange(0, 255);
    ui->horizontalSlider->setRange(0, 255);
    //synchronize spinbox and horizontal slider
    QObject::connect(ui->spinBox, SIGNAL(valueChanged(int)), ui->horizontalSlider,SLOT(setValue(int)));
    QObject::connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), ui->spinBox, SLOT(setValue(int)));
    ui->spinBox->setValue(128);// set the initial value
    //send spinbox value to onSpinValueChanged
    QObject::connect(ui->spinBox, SIGNAL(valueChanged(int)), this, SLOT(horizontal_level(int)));

    ui->spinBox_2->setRange(0, 2000);
    ui->verticalSlider->setRange(0, 2000);
    //synchronize spinbox and horizontal slider
    QObject::connect(ui->spinBox_2, SIGNAL(valueChanged(int)), ui->verticalSlider, SLOT(setValue(int)));
    QObject::connect(ui->verticalSlider, SIGNAL(valueChanged(int)), ui->spinBox_2, SLOT(setValue(int)));
    ui->spinBox_2->setValue(1000);// set the initial value
    //send spinbox value to onSpinValueChanged
    QObject::connect(ui->spinBox_2, SIGNAL(valueChanged(int)), this, SLOT(vertical_window(int)));

    ui->textBrowser->append("\n");
    ui->textBrowser->append("Choose File:\n" + folderDICOM);
    reader_data->SetDirectoryName(folderDICOM.toStdString().c_str());
    reader_data->Update();

    readDICOM(folderDICOM);
}

void MainWindow::readDICOM(const QString& folderDICOM)
{
    // sort the files into a directory
    vtkSmartPointer < vtkDICOMDirectory > dicomdir = vtkSmartPointer < vtkDICOMDirectory >::New();
    dicomdir->SetDirectoryName(folderDICOM.toStdString().c_str());
    dicomdir->RequirePixelDataOn();
    dicomdir->Update();
    int n = dicomdir->GetNumberOfSeries();

    //vtkDICOMreader will read directory above
    vtkSmartPointer <vtkDICOMReader> reader = vtkSmartPointer <vtkDICOMReader>::New();
    reader->SetDataExtent(0, 63, 0, 63, 1, 93);
    reader->SetDataSpacing(3.2, 3.2, 1.5);
    reader->SetDataOrigin(0.0, 0.0, 0.0);
    reader->SetDataScalarTypeToUnsignedShort();
    reader->SetDataByteOrderToLittleEndian();

    if (n > 0)
    {
        // read the first series found
        reader->SetFileNames(dicomdir->GetFileNamesForSeries(0));
        reader->Update();
    }
    else
    {
        //std::cerr << "No DICOM images in directory!" << std::endl;
        QMessageBox::information(this, "About", " No DICOM images in this directory!");
        return;
    }

    reader->SetMemoryRowOrderToFileNative();
    reader->AutoRescaleOff();
    reader->Update();// we need to update the pipeline otherwise nothing will get rendered
    // get the matrix to use when displaying the data
    // (this matrix provides position and orientation)
    vtkMatrix4x4* matrix = reader->GetPatientMatrix();

   // create an image actor to display image and specify orientation
    vtkSmartPointer <vtkImageActor> actor = vtkSmartPointer <vtkImageActor>::New();
    actor->SetUserMatrix(matrix);
    actor->GetMapper()->SetInputConnection(reader->GetOutputPort());



    int imageDims[3];
    reader->GetOutput()->GetDimensions(imageDims);

    vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetInputConnection(reader->GetOutputPort());
    reslice->SetOutputDimensionality(2);// set the iamge as 2D
    reslice->SetResliceAxes(matrix);
    reslice->SetOutputSpacing(reader->GetOutput()->GetSpacing()[0], reader->GetOutput()->GetSpacing()[1], reader->GetOutput()->GetSpacing()[2]);
    reslice->SetOutputOrigin(reader->GetOutput()->GetOrigin()[0], reader->GetOutput()->GetOrigin()[1], reader->GetOutput()->GetOrigin()[2]);
    reslice->SetOutputExtent(reader->GetOutput()->GetExtent());
    reslice->SetInterpolationModeToLinear();//set sampling method

    // Create a greyscale lookup table
    vtkSmartPointer<vtkLookupTable> table = vtkSmartPointer<vtkLookupTable>::New();
    table->SetHueRange(0.0, 0.0);        // image intensity range
    table->SetValueRange(0.0, 1.0);      // from black to white
    table->SetSaturationRange(0.0, 0.0); // no color saturation
    table->SetTableRange(0, 65536);
    table->SetRampToLinear();
    table->Build();

    // Map the image through the lookup table
    vtkSmartPointer<vtkImageMapToColors> color = vtkSmartPointer<vtkImageMapToColors>::New();
    color->SetLookupTable(table);
    color->SetInputConnection(reslice->GetOutputPort());

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
        rep->SetColorMap(color);

        vtkRenderer* renderer;
        if (riw[i] && (renderer = riw[i]->GetRenderer()) != NULL)
        {
            int axis = riw[i]->GetSliceOrientation();
            double vup[3];
            renderer->GetActiveCamera()->GetViewUp(vup);

            double cameraPosition[3];
            renderer->GetActiveCamera()->GetPosition(cameraPosition);
            double cameraFocalPoint[3];
            renderer->GetActiveCamera()->GetPosition(cameraFocalPoint);

            for (int i = 0; i < 3; ++i)
            {
                vup[i] = -vup[i];
                cameraPosition[i] = 2.0 * cameraFocalPoint[i] - cameraPosition[i];
            }
            renderer->GetActiveCamera()->SetPosition(cameraPosition);
            renderer->GetActiveCamera()->SetViewUp(vup);
            renderer->ResetCameraClippingRange();
            riw[i]->SetRenderer(renderer);
            riw[i]->Render();
        }

        riw[i]->SetInputData(reader->GetOutput());
        riw[i]->SetSliceOrientation(i);
        riw[i]->SetResliceModeToAxisAligned();
        riw[i]->SetColorLevel(128.0);// default color level and window when first print out
        riw[i]->SetColorWindow(1000.0);
        riw[i]->SetResliceMode(0);
        riw[i]->GetRenderWindow()->Render();
    }
}

void MainWindow::horizontal_level(int j)// horizontal slider for color level
{
    for (int i = 0; i < 3; i++)
    {
        //pixel value for mid-grey brightness level
        //decreasing color level image brighter
        riw[i]->SetColorLevel(j);
    }

    Render();
    ui->view1->update();
    ui->view2->update();
    ui->view3->update();
}

void MainWindow::vertical_window(int j)//vertical slider for color window
{
    for (int i = 0; i < 3; i++)
    {
        //range of pixel value for displaying width
        //decrrasing color window increases brightness interval
        riw[i]->SetColorWindow(j);
    }

    Render();
    ui->view1->update();
    ui->view2->update();
    ui->view3->update();
}

void MainWindow::refresh()
{
    QMessageBox msgBox;
    msgBox.setText("The document has been modified.");
    msgBox.exec();
    for (int i = 0; i < 3; i++)
    {
        riw[i]->Reset();
        riw[i]->GetRenderer()->ResetCamera();
        riw[i]->Render();
    }
    this->Render();
}

void MainWindow::OpenVTK()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open file"), "",
        "VTK Files (*.vtk)");

    // Open file
    QFile file(fileName);
    file.open(QIODevice::ReadOnly);

    // Return on Cancel
    if (!file.exists())
        return;

    readVTK(fileName);
}

void MainWindow::readVTK(const QString& fileName)
{
    ui->sceneWidget->removeDataSet();

    // Create reader
    vtkSmartPointer<vtkDataSetReader> reader = vtkSmartPointer<vtkDataSetReader>::New();
    reader->SetFileName(fileName.toStdString().c_str());

    // Read the file
    reader->Update();

    // Add data set to 3D view
    vtkSmartPointer<vtkDataSet> dataSet = reader->GetOutput();
    if (dataSet != nullptr) {
        ui->sceneWidget->addDataSet(reader->GetOutput());
    }
}
