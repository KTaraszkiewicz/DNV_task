// mainwindow.cpp - Fixed version with better error handling
#include "mainwindow.h"
#include "../build/ui_mainwindow.h"
#include "glwidget.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QProgressDialog>
#include <QThread>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , glWidget(nullptr)
    , frameCount(0)
    , currentFileName("")
    , frameRateTimer(nullptr)
{
    ui->setupUi(this);
    
    // Set window properties
    setWindowTitle("STL Viewer - 3D Model Viewer");
    setMinimumSize(800, 600);
    resize(1200, 800);
    
    // Create components
    createActions();
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
    connectSignals();
    
    // Initialize frame rate timer
    frameRateTimer = new QTimer(this);
    connect(frameRateTimer, &QTimer::timeout, this, &MainWindow::updateFrameRate);
    frameRateTimer->start(1000); // Update every second
    
    // Set initial status
    statusLabel->setText("Ready - Open an STL file to begin");
    
    qDebug() << "MainWindow: Initialized successfully";
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow: Starting destruction...";
    
    // Stop the frame rate timer first
    if (frameRateTimer) {
        frameRateTimer->stop();
        frameRateTimer = nullptr;
    }
    
    // Clean up GL widget first (this will handle OpenGL cleanup properly)
    if (glWidget) {
        qDebug() << "MainWindow: Cleaning up OpenGL widget...";
        glWidget->setParent(nullptr);  // Remove from parent to prevent double deletion
        delete glWidget;
        glWidget = nullptr;
    }
    
    delete ui;
    
    qDebug() << "MainWindow: Destruction complete";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "MainWindow: Close event received";
    
    // Stop timers
    if (frameRateTimer) {
        frameRateTimer->stop();
    }
    
    // Clean up OpenGL widget before closing
    if (glWidget) {
        // Make sure rendering stops
        glWidget->setParent(nullptr);
    }
    
    // Accept the close event
    event->accept();
    
    // Call parent implementation
    QMainWindow::closeEvent(event);
}

void MainWindow::createActions()
{
    // File menu actions
    openAction = new QAction(QIcon(":/icons/open.png"), "&Open STL...", this);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open an STL file");
    
    exitAction = new QAction(QIcon(":/icons/exit.png"), "E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    
    // Toolbar actions
    resetViewAction = new QAction(QIcon(":/icons/reset.png"), "Reset View", this);
    resetViewAction->setStatusTip("Reset camera to default position");
    
    fitToWindowAction = new QAction(QIcon(":/icons/fit.png"), "Fit to Window", this);
    fitToWindowAction->setStatusTip("Fit model to window");
    
    wireframeAction = new QAction(QIcon(":/icons/wireframe.png"), "Wireframe", this);
    wireframeAction->setCheckable(true);
    wireframeAction->setStatusTip("Toggle wireframe mode");
    
    lightingAction = new QAction(QIcon(":/icons/lighting.png"), "Lighting", this);
    lightingAction->setCheckable(true);
    lightingAction->setChecked(true);
    lightingAction->setStatusTip("Toggle lighting");
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(resetViewAction);
    viewMenu->addAction(fitToWindowAction);
    viewMenu->addSeparator();
    viewMenu->addAction(wireframeAction);
    viewMenu->addAction(lightingAction);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = new QAction("&About", this);
    helpMenu->addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About STL Viewer",
            "STL Viewer v1.0\n\n"
            "A 3D model viewer for STL files\n"
            "Built with Qt6 and OpenGL\n\n"
            "Controls:\n"
            "• Left Mouse: Rotate view\n"
            "• Right Mouse: Pan view\n"
            "• Mouse Wheel: Zoom\n"
            "• Toolbar: Additional controls");
    });
}

void MainWindow::setupToolBar()
{
    QToolBar *mainToolBar = addToolBar("Main");
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    
    // File operations
    mainToolBar->addAction(openAction);
    mainToolBar->addSeparator();
    
    // View operations
    mainToolBar->addAction(resetViewAction);
    mainToolBar->addAction(fitToWindowAction);
    mainToolBar->addSeparator();
    
    // Display modes
    mainToolBar->addAction(wireframeAction);
    mainToolBar->addAction(lightingAction);
    mainToolBar->addSeparator();
    
    // Zoom control
    QWidget *zoomWidget = new QWidget();
    QHBoxLayout *zoomLayout = new QHBoxLayout(zoomWidget);
    zoomLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel *zoomLabel = new QLabel("Zoom:");
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setRange(10, 500);
    zoomSlider->setValue(100);
    zoomSlider->setFixedWidth(100);
    
    zoomSpinBox = new QSpinBox();
    zoomSpinBox->setRange(10, 500);
    zoomSpinBox->setValue(100);
    zoomSpinBox->setSuffix("%");
    zoomSpinBox->setFixedWidth(70);
    
    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addWidget(zoomSlider);
    zoomLayout->addWidget(zoomSpinBox);
    
    mainToolBar->addWidget(zoomWidget);
    mainToolBar->addSeparator();
    
    // Rotation controls
    QWidget *rotationWidget = new QWidget();
    QHBoxLayout *rotationLayout = new QHBoxLayout(rotationWidget);
    rotationLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel *rotLabel = new QLabel("Rotation:");
    rotationXSlider = new QSlider(Qt::Horizontal);
    rotationXSlider->setRange(-180, 180);
    rotationXSlider->setValue(0);
    rotationXSlider->setFixedWidth(80);
    
    rotationYSlider = new QSlider(Qt::Horizontal);
    rotationYSlider->setRange(-180, 180);
    rotationYSlider->setValue(0);
    rotationYSlider->setFixedWidth(80);
    
    rotationZSlider = new QSlider(Qt::Horizontal);
    rotationZSlider->setRange(-180, 180);
    rotationZSlider->setValue(0);
    rotationZSlider->setFixedWidth(80);
    
    rotationLayout->addWidget(rotLabel);
    rotationLayout->addWidget(new QLabel("X:"));
    rotationLayout->addWidget(rotationXSlider);
    rotationLayout->addWidget(new QLabel("Y:"));
    rotationLayout->addWidget(rotationYSlider);
    rotationLayout->addWidget(new QLabel("Z:"));
    rotationLayout->addWidget(rotationZSlider);
    
    mainToolBar->addWidget(rotationWidget);
}

void MainWindow::setupCentralWidget()
{
    // Create the OpenGL widget
    glWidget = new GLWidget(this);
    
    // Create a central widget with layout
    QWidget *centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Add the OpenGL widget
    mainLayout->addWidget(glWidget);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
    
    // File info label
    fileInfoLabel = new QLabel("No file loaded");
    fileInfoLabel->setMinimumWidth(300);
    statusBar()->addWidget(fileInfoLabel);
    
    // Status label
    statusLabel = new QLabel("Ready");
    statusBar()->addPermanentWidget(statusLabel);
    
    // Frame rate label
    frameRateLabel = new QLabel("FPS: 0");
    frameRateLabel->setMinimumWidth(80);
    statusBar()->addPermanentWidget(frameRateLabel);
}

void MainWindow::connectSignals()
{
    // File menu connections
    connect(openAction, &QAction::triggered, this, &MainWindow::openSTLFile);
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    
    // Toolbar connections
    connect(resetViewAction, &QAction::triggered, this, &MainWindow::resetView);
    connect(fitToWindowAction, &QAction::triggered, this, &MainWindow::fitToWindow);
    connect(wireframeAction, &QAction::triggered, this, &MainWindow::toggleWireframe);
    connect(lightingAction, &QAction::triggered, this, &MainWindow::toggleLighting);
    
    // Slider connections
    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomChanged);
    connect(zoomSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            zoomSlider, &QSlider::setValue);
    connect(zoomSlider, &QSlider::valueChanged, zoomSpinBox, &QSpinBox::setValue);
    
    connect(rotationXSlider, &QSlider::valueChanged, this, &MainWindow::onRotationXChanged);
    connect(rotationYSlider, &QSlider::valueChanged, this, &MainWindow::onRotationYChanged);
    connect(rotationZSlider, &QSlider::valueChanged, this, &MainWindow::onRotationZChanged);

    // OpenGL widget connections
    if (glWidget) {
        connect(glWidget, &GLWidget::frameRendered, this, [this]{ frameCount++; });
        connect(glWidget, &GLWidget::fileLoaded, this, &MainWindow::updateFileInfo);
    }
}

// Slot implementations
void MainWindow::openSTLFile()
{
    qDebug() << "MainWindow: Opening STL file dialog...";
    
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open STL File", 
        QDir::homePath(),
        "STL Files (*.stl);;All Files (*)");

    if (!fileName.isEmpty()) {
        qDebug() << "MainWindow: Selected file:" << fileName;
        
        if (!glWidget) {
            QMessageBox::critical(this, "Error", "OpenGL widget not initialized");
            return;
        }
        
        // Disable UI during loading
        setEnabled(false);
        statusLabel->setText("Loading STL file...");
        QApplication::processEvents();
        
        try {
            // Load the file (this should run on main thread)
            glWidget->loadSTLFile(fileName);
            currentFileName = fileName;
            statusLabel->setText("File loaded successfully");
            
        } catch (const std::exception& e) {
            QString errorMsg = QString("Error loading STL file: %1").arg(e.what());
            QMessageBox::critical(this, "Load Error", errorMsg);
            statusLabel->setText("Failed to load file");
            qCritical() << errorMsg;
            
        } catch (...) {
            QString errorMsg = "Unknown error occurred while loading STL file";
            QMessageBox::critical(this, "Load Error", errorMsg);
            statusLabel->setText("Failed to load file");
            qCritical() << errorMsg;
        }
        
        // Re-enable UI
        setEnabled(true);
        
    } else {
        qDebug() << "MainWindow: File dialog cancelled";
    }
}

void MainWindow::exitApplication()
{
    qDebug() << "MainWindow: Exit requested";
    
    // Stop all timers first
    if (frameRateTimer) {
        frameRateTimer->stop();
    }
    
    // Clean shutdown
    close();
}

void MainWindow::resetView()
{
    if (glWidget) {
        glWidget->resetCamera();
        // Reset sliders
        zoomSlider->setValue(100);
        rotationXSlider->setValue(0);
        rotationYSlider->setValue(0);
        rotationZSlider->setValue(0);
        statusLabel->setText("View reset");
        qDebug() << "MainWindow: View reset";
    }
}

void MainWindow::fitToWindow()
{
    if (glWidget) {
        glWidget->fitToWindow();
        statusLabel->setText("Fitted to window");
        qDebug() << "MainWindow: Fitted to window";
    }
}

void MainWindow::toggleWireframe()
{
    if (glWidget) {
        bool wireframe = wireframeAction->isChecked();
        glWidget->setWireframeMode(wireframe);
        statusLabel->setText(wireframe ? "Wireframe mode enabled" : "Wireframe mode disabled");
        qDebug() << "MainWindow: Wireframe mode" << (wireframe ? "enabled" : "disabled");
    }
}

void MainWindow::toggleLighting()
{
    if (glWidget) {
        bool lighting = lightingAction->isChecked();
        glWidget->setLightingEnabled(lighting);
        statusLabel->setText(lighting ? "Lighting enabled" : "Lighting disabled");
        qDebug() << "MainWindow: Lighting" << (lighting ? "enabled" : "disabled");
    }
}

void MainWindow::onZoomChanged(int value)
{
    if (glWidget) {
        float zoomFactor = value / 100.0f;
        glWidget->setZoom(zoomFactor);
    }
}

void MainWindow::onRotationXChanged(int value)
{
    if (glWidget) {
        glWidget->setRotationX(value);
    }
}

void MainWindow::onRotationYChanged(int value)
{
    if (glWidget) {
        glWidget->setRotationY(value);
    }
}

void MainWindow::onRotationZChanged(int value)
{
    if (glWidget) {
        glWidget->setRotationZ(value);
    }
}

void MainWindow::updateFrameRate()
{
    if (frameRateLabel) {
        frameRateLabel->setText(QString("FPS: %1").arg(frameCount));
        frameCount = 0;
    }
}

void MainWindow::updateFileInfo(const QString& filename, int triangles, int vertices)
{
    QString info = QString("%1 - Triangles: %2, Vertices: %3")
                   .arg(filename)
                   .arg(triangles)
                   .arg(vertices);
    
    if (fileInfoLabel) {
        fileInfoLabel->setText(info);
    }
    
    qDebug() << "MainWindow: File info updated:" << info;
}