#include "src/editors/EditorBase.h"
#include "qdir.h"

// Constructs the editor.
// compatibilityManager - manager that should be used to transform data between
//                        various data types using compatibility layers
// filePath - absolute file path of the file that should be opened
EditorBase::EditorBase(/*compatibilityManager, */ const QString& filePath)
{
    _filePath = QDir::cleanPath(filePath);
    _labelText = QFileInfo(filePath).fileName(); //.baseName();
/*
        self.compatibilityManager = compatibilityManager
        self.desiredSavingDataType = "" if self.compatibilityManager is None else self.compatibilityManager.EditorNativeType
        self.nativeData = None

        self.initialised = False
        self.active = False

        self.mainWindow = None

        #Set up a QFileSystemWatcher to watch for external changes to a file
        self.fileMonitor = None
        self.fileChangedByExternalProgram = False
        self.displayingReloadAlert = False
*/
}

// This method loads everything up so this editor is ready to be switched to
//???all mainWindow related things must be done externally in a main window?
void EditorBase::initialize(/*mainWindow*/)
{
    assert(!_initialized);
/*
        self.mainWindow = mainWindow
        self.tabWidget.tabbedEditor = self

        if self.compatibilityManager is not None:
            rawData = codecs.open(self.filePath, mode = "r", encoding = "utf-8").read()
            rawDataType = ""

            if rawData == "":
                # it's an empty new file, the derived classes deal with this separately
                self.nativeData = rawData

                if mainWindow.project is None:
                    self.desiredSavingDataType = self.compatibilityManager.EditorNativeType
                else:
                    self.desiredSavingDataType = self.compatibilityManager.getSuitableDataTypeForCEGUIVersion(mainWindow.project.CEGUIVersion)

            else:
                try:
                    rawDataType = self.compatibilityManager.guessType(rawData, self.filePath)

                    # A file exists and the editor has it open, so watch it for
                    # external changes.
                    self.addFileMonitor(self.filePath)

                except compatibility.NoPossibleTypesError:
                    dialog = NoTypeDetectedDialog(self.compatibilityManager)
                    result = dialog.exec_()

                    rawDataType = self.compatibilityManager.EditorNativeType
                    self.nativeData = ""

                    if result == QtGui.QDialog.Accepted:
                        selection = dialog.typeChoice.selectedItems()

                        if len(selection) == 1:
                            rawDataType = selection[0].text()
                            self.nativeData = None

                except compatibility.MultiplePossibleTypesError as e:
                    # if no project is opened or if the opened file was detected as something not suitable for the target CEGUI version of the project
                    if (mainWindow.project is None) or (self.compatibilityManager.getSuitableDataTypeForCEGUIVersion(mainWindow.project.CEGUIVersion) not in e.possibleTypes):
                        dialog = MultipleTypesDetectedDialog(self.compatibilityManager, e.possibleTypes)
                        result = dialog.exec_()

                        rawDataType = self.compatibilityManager.EditorNativeType
                        self.nativeData = ""

                        if result == QtGui.QDialog.Accepted:
                            selection = dialog.typeChoice.selectedItems()

                            if len(selection) == 1:
                                rawDataType = selection[0].text()
                                self.nativeData = None

                    else:
                        rawDataType = self.compatibilityManager.getSuitableDataTypeForCEGUIVersion(mainWindow.project.CEGUIVersion)
                        self.nativeData = None

                # by default, save in the same format as we opened in
                self.desiredSavingDataType = rawDataType

                if mainWindow.project is not None:
                    projectCompatibleDataType = self.compatibilityManager.CEGUIVersionTypes[mainWindow.project.CEGUIVersion]

                    if projectCompatibleDataType != rawDataType:
                        if QtGui.QMessageBox.question(mainWindow,
                                                      "Convert to format suitable for opened project?",
                                                      "File you are opening isn't suitable for the project that is opened at the moment.\n"
                                                      "Do you want to convert it to a suitable format upon saving?\n"
                                                      "(from '%s' to '%s')\n"
                                                      "Data COULD be lost, make a backup!)" % (rawDataType, projectCompatibleDataType),
                                                      QtGui.QMessageBox.Yes | QtGui.QMessageBox.No, QtGui.QMessageBox.No) == QtGui.QMessageBox.Yes:
                            self.desiredSavingDataType = projectCompatibleDataType

                # if nativeData is "" at this point, data type was not successful and user didn't select
                # any data type as well so we will just use given file as an empty file

                if self.nativeData != "":
                    try:
                        self.nativeData = self.compatibilityManager.transform(rawDataType, self.compatibilityManager.EditorNativeType, rawData)

                    except compatibility.LayerNotFoundError:
                        # TODO: Dialog, can't convert
                        self.nativeData = ""

        self.initialised = True
*/
}

void EditorBase::finalize()
{
/*
        """Cleans up after itself
        this is usually called when you want the tab closed
        """

        assert(self.initialised)
        assert(self.tabWidget)

        self.initialised = False
*/
}

void EditorBase::reloadData()
{
/*
        """Reinitialises this tabbed editor, effectivelly reloading the file
        off the hard drive again
        """

        wasCurrent = self.mainWindow.activeEditor is self

        mainWindow = self.mainWindow
        self.finalise()
        self.initialise(mainWindow)

        if wasCurrent:
            self.makeCurrent()
*/
}

void EditorBase::destroy()
{
/*
        """Removes itself from the tab list and irrevocably destroys
        data associated with itself
        """

        i = 0
        wdt = self.mainWindow.tabs.widget(i)
        tabRemoved = False

        while wdt:
            if wdt == self.tabWidget:
                self.mainWindow.tabs.removeTab(i)
                tabRemoved = True
                break

            i = i + 1
            wdt = self.mainWindow.tabs.widget(i)

        assert(tabRemoved)
*/
}

// Causes the tabbed editor to save all it's progress to the file.
// targetPath should be absolute file path.
bool EditorBase::saveAs(const QString& targetPath, bool updateCurrentPath)
{
/*
        outputData = self.nativeData if self.nativeData is not None else ""
        if self.compatibilityManager is not None:
            outputData = self.compatibilityManager.transform(self.compatibilityManager.EditorNativeType, self.desiredSavingDataType, self.nativeData)

        # Stop monitoring the file, the changes that are about to occur are not
        # picked up as being from an external program!
        self.removeFileMonitor(self.filePath)

        try:
            f = codecs.open(targetPath, mode = "w", encoding = "utf-8")
            f.write(outputData)
            f.close()

        except IOError as e:
            # The rest of the code is skipped, so be sure to turn file
            # monitoring back on
            self.addFileMonitor(self.filePath)
            QtGui.QMessageBox.critical(self, "Error saving file!",
                                       "CEED encountered an error trying to save the file.\n\n(exception details: %s)" % (e))
            return False

            //???use signal to update UI?
        if updateCurrentPath:
            # changes current path to the path we saved to
            self.filePath = targetPath

            # update tab text
            self.tabLabel = os.path.basename(self.filePath)

            # because this might be called even before initialise is called!
            if self.mainWindow is not None:
                self.mainWindow.tabs.setTabText(self.mainWindow.tabs.indexOf(self.tabWidget), self.tabLabel)

        self.addFileMonitor(self.filePath)
*/
    return true;
}