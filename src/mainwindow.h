#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QSpinBox>
#include <QGroupBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class GLWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File menu actions
    void openSTLFile();
    void exitApplication();
    
    // Toolbar actions
    void resetView();
    void fitToWindow();
    void toggleWireframe();
    void toggleLighting();
    
    // View controls
    void onZoomChanged(int value);
    void onRotationXChanged(int value);
    void onRotationYChanged(int value);
    void onRotationZChanged(int value);
    
    // Status updates
    void updateFrameRate();
    void updateFileInfo(const QString& filename, int triangles, int vertices);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void createActions();
    void connectSignals();
    
    // UI components
    Ui::MainWindow *ui;
    GLWidget *glWidget;
    
    // Menu actions
    QAction *openAction;
    QAction *exitAction;
    
    // Toolbar actions
    QAction *resetViewAction;
    QAction *fitToWindowAction;
    QAction *wireframeAction;
    QAction *lightingAction;
    
    // Toolbar controls
    QSlider *zoomSlider;
    QSlider *rotationXSlider;
    QSlider *rotationYSlider;
    QSlider *rotationZSlider;
    QSpinBox *zoomSpinBox;
    
    // Status bar widgets
    QLabel *fileInfoLabel;
    QLabel *frameRateLabel;
    QLabel *statusLabel;
    
    // Timer for frame rate calculation
    QTimer *frameRateTimer;
    int frameCount;
    
    // Current file info
    QString currentFileName;
};

#endif // MAINWINDOW_H