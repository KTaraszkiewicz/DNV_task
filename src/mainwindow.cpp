// Main application window - handles UI, menus, and controls
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
    
    // Configure main window appearance
    setWindowTitle("STL Viewer - 3D Model Viewer");
    setMinimumSize(800, 600);
    resize(1200, 800);
    
    // Build the user interface
    createActions();          // Create menu and toolbar actions
    setupMenuBar();          // Set up File, View, Help menus
    setupToolBar();          // Set up toolbar with buttons and sliders
    setupCentralWidget();    // Create the main OpenGL display area
    setupStatusBar();        // Set up status bar at bottom
    connectSignals();        // Connect UI controls to their functions
    
    // Set up frame rate counter (updates every second)
    frameRateTimer = new QTimer(this);
    connect(frameRateTimer, &QTimer::timeout, this, &MainWindow::updateFrameRate);
    frameRateTimer->start(1000);
    
    // Show initial status
    statusLabel->setText("Ready - Open an STL file to begin");
    
    qDebug() << "MainWindow: Initialized successfully";
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow: Starting destruction...";
    
    // Stop frame rate timer
    if (frameRateTimer) {
        frameRateTimer->stop();
        frameRateTimer = nullptr;
    }
    
    // Clean up OpenGL widget (this handles all OpenGL resource cleanup)
    if (glWidget) {
        qDebug() << "MainWindow: Cleaning up OpenGL widget...";
        glWidget->setParent(nullptr);  // Prevent double deletion
        delete glWidget;
        glWidget = nullptr;
    }
    
    delete ui;
    
    qDebug() << "MainWindow: Destruction complete";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "MainWindow: Close event received";
    
    // Stop all timers
    if (frameRateTimer) {
        frameRateTimer->stop();
    }
    
    // Prepare OpenGL widget for shutdown
    if (glWidget) {
        glWidget->setParent(nullptr);
    }
    
    // Accept the close event
    event->accept();
    
    // Let parent class handle the rest
    QMainWindow::closeEvent(event);
}

void MainWindow::createActions()
{
    // File menu actions
    openAction = new QAction(QIcon(":/icons/open.png"), "&Open STL...", this);
    openAction->setShortcut(QKeySequence::Open);  // Ctrl+O
    openAction->setStatusTip("Open an STL file");
    
    exitAction = new QAction(QIcon(":/icons/exit.png"), "E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);  // Ctrl+Q
    exitAction->setStatusTip("Exit the application");
    
    // View control actions
    resetViewAction = new QAction(QIcon(":/icons/reset.png"), "Reset View", this);
    resetViewAction->setStatusTip("Reset camera to default position");
    
    fitToWindowAction = new QAction(QIcon(":/icons/fit.png"), "Fit to Window", this);
    fitToWindowAction->setStatusTip("Fit model to window");
    
    // Display mode actions
    wireframeAction = new QAction(QIcon(":/icons/wireframe.png"), "Wireframe", this);
    wireframeAction->setCheckable(true);          // Can be toggled on/off
    wireframeAction->setStatusTip("Toggle wireframe mode");
    
    lightingAction = new QAction(QIcon(":/icons/lighting.png"), "Lighting", this);
    lightingAction->setCheckable(true);
    lightingAction->setChecked(true);             // Start with lighting enabled
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
    
    // Help menu with about dialog
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
    
    // Zoom control section with slider and spinbox
    QWidget *zoomWidget = new QWidget();
    QHBoxLayout *zoomLayout = new QHBoxLayout(zoomWidget);
    zoomLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel *zoomLabel = new QLabel("Zoom:");
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setRange(10, 500);        // 10% to 500% zoom
    zoomSlider->setValue(100);            // Start at 100%
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
    
    // Rotation control section with X, Y, Z sliders
    QWidget *rotationWidget = new QWidget();
    QHBoxLayout *rotationLayout = new QHBoxLayout(rotationWidget);
    rotationLayout->setContentsMargins(5, 5, 5, 5);
    
    QLabel *rotLabel = new QLabel("Rotation:");
    
    // X-axis rotation
    rotationXSlider = new QSlider(Qt::Horizontal);
    rotationXSlider->setRange(-180, 180);
    rotationXSlider->setValue(0);
    rotationXSlider->setFixedWidth(80);
    
    // Y-axis rotation
    rotationYSlider = new QSlider(Qt::Horizontal);
    rotationYSlider->setRange(-180, 180);
    rotationYSlider->setValue(0);
    rotationYSlider->setFixedWidth(80);
    
    // Z-axis rotation
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
    // Create the main OpenGL rendering widget
    glWidget = new GLWidget(this);
    
    // Create container widget with layout
    QWidget *centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // No margins for full-screen OpenGL
    
    // Add OpenGL widget to fill entire central area
    mainLayout->addWidget(glWidget);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
    
    // File information display (left side)
    fileInfoLabel = new QLabel("No file loaded");
    fileInfoLabel->setMinimumWidth(300);
    statusBar()->addWidget(fileInfoLabel);
    
    // General status messages (center)
    statusLabel = new QLabel("Ready");
    statusBar()->addPermanentWidget(statusLabel);
    
    // Frame rate display (right side)
    frameRateLabel = new QLabel("FPS: 0");
    frameRateLabel->setMinimumWidth(80);
    statusBar()->addPermanentWidget(frameRateLabel);
}

void MainWindow::connectSignals()
{
    // Connect menu actions to their functions
    connect(openAction, &QAction::triggered, this, &MainWindow::openSTLFile);
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    
    // Connect toolbar actions
    connect(resetViewAction, &QAction::triggered, this, &MainWindow::resetView);
    connect(fitToWindowAction, &QAction::triggered, this, &MainWindow::fitToWindow);
    connect(wireframeAction, &QAction::triggered, this, &MainWindow::toggleWireframe);
    connect(lightingAction, &QAction::triggered, this, &MainWindow::toggleLighting);
    
    // Connect zoom controls (slider and spinbox stay synchronized)
    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomChanged);
    connect(zoomSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            zoomSlider, &QSlider::setValue);
    connect(zoomSlider, &QSlider::valueChanged, zoomSpinBox, &QSpinBox::setValue);
    
    // Connect rotation sliders
    connect(rotationXSlider, &QSlider::valueChanged, this, &MainWindow::onRotationXChanged);
    connect(rotationYSlider, &QSlider::valueChanged, this, &MainWindow::onRotationYChanged);
    connect(rotationZSlider, &QSlider::valueChanged, this, &MainWindow::onRotationZChanged);

    // Connect OpenGL widget signals
    if (glWidget) {
        // Count rendered frames for FPS calculation
        connect(glWidget, &GLWidget::frameRendered, this, [this]{ frameCount++; });
        // Update status when file loads
        connect(glWidget, &GLWidget::fileLoaded, this, &MainWindow::updateFileInfo);
    }
}

// File menu functions
void MainWindow::openSTLFile()
{
    qDebug() << "MainWindow: Opening STL file dialog...";
    
    // Show file picker dialog
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
        
        // Disable UI during loading to prevent interference
        setEnabled(false);
        statusLabel->setText("Loading STL file...");
        QApplication::processEvents();  // Update UI immediately
        
        try {
            // Load the file through OpenGL widget
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
    
    // Stop all timers before closing
    if (frameRateTimer) {
        frameRateTimer->stop();
    }
    
    // Close the application
    close();
}

// View control functions
void MainWindow::resetView()
{
    if (glWidget) {
        glWidget->resetCamera();
        // Reset UI controls to match
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

// Slider control functions
void MainWindow::onZoomChanged(int value)
{
    if (glWidget) {
        float zoomFactor = value / 100.0f;  // Convert percentage to decimal
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
        frameCount = 0;  // Reset counter for next second
    }
}

void MainWindow::updateFileInfo(const QString& filename, int triangles, int vertices)
{
    // Format file information string
    QString info = QString("%1 - Triangles: %2, Vertices: %3")
                   .arg(filename)
                   .arg(triangles)
                   .arg(vertices);
    
    if (fileInfoLabel) {
        fileInfoLabel->setText(info);
    }
    
    qDebug() << "MainWindow: File info updated:" << info;
}