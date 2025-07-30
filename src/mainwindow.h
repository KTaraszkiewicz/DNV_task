#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
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
    // What happens when user clicks "Open" or "Exit" in the menu
    void openSTLFile();
    void exitApplication();
    
    // What happens when user clicks toolbar buttons
    void resetView();          // Reset camera to default position
    void fitToWindow();        // Zoom to show entire 3D model
    void toggleWireframe();    // Switch between solid and wireframe view
    void toggleLighting();     // Turn lights on/off
    
    // What happens when user moves the control sliders
    void onZoomChanged(int value);        // User zoomed in or out
    void onRotationXChanged(int value);   // User rotated around X axis
    void onRotationYChanged(int value);   // User rotated around Y axis
    void onRotationZChanged(int value);   // User rotated around Z axis
    
    // Keep the display updated with current info
    void updateFrameRate();               // Show how fast we're drawing frames
    void updateFileInfo(const QString& filename, int triangles, int vertices);

private:
    // Build the different parts of the window
    void setupMenuBar();      // Create File menu, etc.
    void setupToolBar();      // Create buttons at the top
    void setupStatusBar();    // Create info bar at the bottom
    void setupCentralWidget(); // Create the main 3D viewing area
    void createActions();     // Set up what menu items and buttons do
    void connectSignals();    // Wire up sliders to their functions
    
    // The main parts of our window
    Ui::MainWindow *ui;
    GLWidget *glWidget;       // This is where we draw the 3D model
    
    // Menu items that user can click
    QAction *openAction;      // Open STL file
    QAction *exitAction;      // Quit the program
    
    // Toolbar buttons
    QAction *resetViewAction;    // Reset camera button
    QAction *fitToWindowAction;  // Fit to window button
    QAction *wireframeAction;    // Wireframe toggle button
    QAction *lightingAction;     // Lighting toggle button
    
    // User controls for manipulating the view
    QSlider *zoomSlider;         // Slider to zoom in/out
    QSlider *rotationXSlider;    // Slider to rotate left/right
    QSlider *rotationYSlider;    // Slider to rotate up/down
    QSlider *rotationZSlider;    // Slider to spin around
    QSpinBox *zoomSpinBox;       // Number box showing exact zoom level
    
    // Information displayed at bottom of window
    QLabel *fileInfoLabel;       // Shows filename and model statistics
    QLabel *frameRateLabel;      // Shows how many frames per second
    QLabel *statusLabel;         // Shows current status messages
    
    // Timer that triggers frame rate calculation every second
    QTimer *frameRateTimer;
    int frameCount;              // Count frames to calculate FPS
    
    // Keep track of what file we have open
    QString currentFileName;

protected:
    // What to do when user tries to close the window
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H