#include "src/proj/CEGUIProjectManager.h"
#include "CEGUIProject.h"

CEGUIProjectManager::CEGUIProjectManager()
{

}

void CEGUIProjectManager::createProject()
{
/*
    # creates the project using data from this dialog
    def createProject(self):
        ret = Project()
        ret.projectFilePath = self.projectFilePath.text()

        if not ret.projectFilePath.endswith(".project"):
            # enforce the "project" extension
            ret.projectFilePath += ".project"

        if self.createResourceDirs.checkState() == QtCore.Qt.Checked:
            try:
                prefix = os.path.dirname(ret.projectFilePath)
                dirs = ["fonts", "imagesets", "looknfeel", "schemes", "layouts", "xml_schemas"]

                for dir_ in dirs:
                    if not os.path.exists(os.path.join(prefix, dir_)):
                        os.mkdir(os.path.join(prefix, dir_))

            except OSError as e:
                QtGui.QMessageBox.critical(self, "Cannot create resource \
directories!", "There was a problem creating the resource \
directories.  Do you have the proper permissions on the \
parent directory? (exception info: %s)" % (e))

        return ret
*/
}

// Opens the project file given in 'path'. Assumes no project is opened at the point this is called.
// Caller must test if a project is opened and close it accordingly (with a dialog
// being shown if there are changes to it)
// Errors aren't indicated by exceptions or return values, dialogs are shown in case of errors.
// path - Absolute path of the project file
// openSettings - if True, the settings dialog is opened instead of just loading the resources,
//               this is desirable when creating a new project
void CEGUIProjectManager::loadProject(const QString& fileName)
{
    assert(loadedProjectFileName.isEmpty());
    if (!loadedProjectFileName.isEmpty()) return;

    CEGUIProject proj;
    proj.loadFromFile(fileName);
    // load xml file
    // create CEGUIProject from xml
    /*
        self.project = project.Project()
        try:
            self.project.load(path)
        except IOError:
            QtGui.QMessageBox.critical(self, "Error when opening project", "It seems project at path '%s' doesn't exist or you don't have rights to open it." % (path))

            self.project = None
            return
    */

    if (!proj.checkAllDirectories())
    {
        /*
        if (indicateErrorsWithDialogs)
            QtGui.QMessageBox.warning(self, "At least one of project's resource directories is invalid",
            "Project's resource directory paths didn't pass the sanity check, please check projects settings.
            "Details of this error: %s");
        */
        return;
    }

    /*
        if openSettings:
            self.slot_projectSettings()

        else:
            self.syncProjectToCEGUIInstance()
    */

    //!!!see updateUIOnProjectChanged for UI changes!

    loadedProjectFileName = fileName;
}

void CEGUIProjectManager::unloadProject()
{
    loadedProjectFileName.clear();
}
