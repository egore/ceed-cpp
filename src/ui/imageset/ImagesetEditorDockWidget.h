#ifndef IMAGESETEDITORDOCKWIDGET_H
#define IMAGESETEDITORDOCKWIDGET_H

#include <QDockWidget>

// Provides list of images, property editing of currently selected image and create/delete

class ImagesetEntry;
class ImageEntry;
class QListWidgetItem;

namespace Ui {
class ImagesetEditorDockWidget;
}

class ImagesetEditorDockWidget : public QDockWidget
{
    Q_OBJECT

public:

    explicit ImagesetEditorDockWidget(QWidget *parent = nullptr);
    ~ImagesetEditorDockWidget();

    void setImagesetEntry(ImagesetEntry* entry) { imagesetEntry = entry; }
    void setActiveImageEntry(ImageEntry* entry);
    void refreshActiveImageEntry();

    void refresh();

private slots:

    void onNativeResolutionEdited();
    void onNativeResolutionPerImageEdited();

    void on_name_textEdited(const QString &arg1);

    void on_imageLoad_clicked();

    void on_autoScaled_currentIndexChanged(int index);

    void on_autoScaledPerImage_currentIndexChanged(int index);

    void on_filterBox_textChanged(const QString &arg1);

    void on_list_itemChanged(QListWidgetItem *item);

    void on_list_itemSelectionChanged();

    void on_positionX_textChanged(const QString &arg1);

    void on_positionY_textChanged(const QString &arg1);

    void on_width_textChanged(const QString &arg1);

    void on_height_textChanged(const QString &arg1);

    void on_offsetX_textChanged(const QString &arg1);

    void on_offsetY_textChanged(const QString &arg1);

private:

    virtual void keyReleaseEvent(QKeyEvent* event) override;

    Ui::ImagesetEditorDockWidget *ui;
    ImagesetEntry* imagesetEntry = nullptr;

    bool selectionUnderway = false;
    bool selectionSynchronizationUnderway = false;
};

#endif // IMAGESETEDITORDOCKWIDGET_H
