#ifndef IMAGESETENTRY_H
#define IMAGESETENTRY_H

#include "qgraphicsitem.h"

// This is the whole imageset containing all the images (ImageEntries).
// The main reason for this is not to have multiple imagesets editing at once but rather
// to have the transparency background working properly.

class QDomElement;
class ImageEntry;
class QFileSystemWatcher;
class ImagesetVisualMode;

class ImagesetEntry : public QGraphicsPixmapItem
{
public:

    ImagesetEntry(ImagesetVisualMode& visualMode);
    ~ImagesetEntry() override;

    void loadFromElement(const QDomElement& xml);
    void saveToElement(QDomElement& xml);

    ImageEntry* getImageEntry(const QString& name) const;
    bool showOffsets() const { return _showOffsets; }
    void setShowOffsets(bool value) { _showOffsets = value; }

    QString getAbsoluteImageFile() const;
    QString convertToRelativeImageFile(const QString& absPath) const;

protected:// slots:

    void onImageChangedByExternalProgram();

protected:

    void loadImage(const QString& relPath);

    ImagesetVisualMode& _visualMode;

    QString _name = "Unknown";
    QString imageFile;
    QString autoScaled = "false";
    int nativeHorzRes = 800;
    int nativeVertRes = 600;
    bool _showOffsets = false;

    std::vector<ImageEntry*> imageEntries;

    QGraphicsRectItem* transparencyBackground = nullptr;

    //???here or in MainWindow?
    QFileSystemWatcher* imageMonitor = nullptr;
    bool displayingReloadAlert = false;
};

#endif // IMAGESETENTRY_H