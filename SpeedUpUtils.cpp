
#include <Core/CoreAll.h>
#include <Fusion/FusionAll.h>
#include <Cam/CamAll.h>
#include <string>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace adsk::core;
using namespace adsk::fusion;
// using namespace adsk::cam;

// Cross-platform export macro
#ifdef _WIN32
#define XI_EXPORT __declspec(dllexport)
#else
#define XI_EXPORT __attribute__((visibility("default")))
#endif

Ptr<ExtrudeFeature> extrudeSemicircle(Ptr<Component>, Ptr<Sketch>);
bool checkReturn(Ptr<Base>);

Ptr<Application> app;
Ptr<UserInterface> ui;
Ptr<ToolbarPanelList> toolbarPanelList;
Ptr<ToolbarControlList> toolbarControlList_var;

// Parameters with placeholder values
double radius = 2.5;      // 0.1 cm = 1 mm (API uses centimeters)
double sweepAngle = M_PI; // 180 degrees in radians (π radians)
double thickness = 1.0;

extern "C" XI_EXPORT bool run(const char *context)
{
    Ptr<ObjectCollection> bodies = ObjectCollection::create();
    if (!bodies)
        return false;

    app = Application::get();
    if (!app)
        return false;

    ui = app->userInterface();
    if (!ui)
        return false;

    Ptr<Documents> documents = app->documents();
    if (!documents)
        return false;

    Ptr<Document> doc = documents->add(DocumentTypes::FusionDesignDocumentType);
    if (!doc)
        return false;

    Ptr<Product> product = app->activeProduct();
    if (!product)
        return false;

    Ptr<Design> design = product;
    if (!design)
        return false;

    // Get the root component of the active design
    Ptr<Component> rootComp = design->rootComponent();
    if (!rootComp)
        return false;

    // Create sketch
    Ptr<Sketches> sketches = rootComp->sketches();
    if (!sketches)
        return false;
    Ptr<ConstructionPlane> xz = rootComp->xZConstructionPlane();
    if (!xz)
        return false;
    Ptr<Sketch> sketch = sketches->add(xz);
    if (!sketch)
        return false;
    Ptr<SketchCurves> sketchCurves = sketch->sketchCurves();
    if (!sketchCurves)
        return false;
    Ptr<SketchLines> sketchLines = sketchCurves->sketchLines();
    if (!sketchLines)
        return false;
    Ptr<Point3D> startPoint = Point3D::create(radius, 0, 0);
    if (!startPoint)
        return false;
    Ptr<Point3D> endPoint = Point3D::create(-radius, 0, 0);
    if (!endPoint)
        return false;
    sketchLines->addByTwoPoints(startPoint, endPoint);

    // Create center point at origin
    Ptr<Point3D> centerPoint = Point3D::create(0, 0, 0);
    if (!centerPoint)
        return false;

    Ptr<SketchArcs> sketchArcs = sketchCurves->sketchArcs();
    if (!sketchArcs)
        return false;

    Ptr<SketchArc> arc = sketchArcs->addByCenterStartSweep(centerPoint, startPoint, sweepAngle);
    if (!arc)
        return false;

    Ptr<ExtrudeFeature> extrusion = extrudeSemicircle(rootComp, sketch);
    if (!extrusion)
        return false;

    // Since the sketch of the semicircle is created with its straight edge parallel with one of the origin axes,
    // the extrusion is orthogonal to the axis. To create a cyllinder, select the plane orthogonal to both.
    // In this case, the XY-plane

    Ptr<ConstructionPlane> xy = rootComp->xYConstructionPlane();
    if (!xy)
        return false;

    // Select the newly extruded body for mirror operation
    bodies->add(extrusion->bodies()->item(0));
    if (!checkReturn(bodies))
        return false;

    Ptr<MirrorFeatureInput> mirrorInput = rootComp->features()->mirrorFeatures()->createInput(bodies, xy);
    if (!checkReturn(mirrorInput))
        return false;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeature = rootComp->features()->mirrorFeatures()->add(mirrorInput);
    if (!checkReturn(mirrorFeature))
        return false;

    return true;
}

bool checkReturn(Ptr<Base> returnObj)
{
    if (returnObj)
        return true;
    else if (app && ui)
    {
        std::string errDesc;
        app->getLastError(&errDesc);
        ui->messageBox(errDesc);
        return false;
    }
    else
        return false;
}

Ptr<ExtrudeFeature> extrudeSemicircle(Ptr<Component> component, Ptr<Sketch> sketch)
{
    Ptr<ExtrudeFeatures> extrudes = component->features()->extrudeFeatures();
    if (!checkReturn(extrudes))
        return nullptr;

    if (sketch->profiles()->count() == 0)
        return nullptr;
    
    Ptr<Profile> prof = sketch->profiles()->item(0);
    if (!checkReturn(prof))
        return nullptr;

    Ptr<ExtrudeFeatureInput> extInput = extrudes->createInput(prof, NewBodyFeatureOperation);
    if (!checkReturn(extInput))
        return nullptr;

    Ptr<ValueInput> distance = adsk::core::ValueInput::createByReal(thickness);
    if (!checkReturn(distance))
        return nullptr;

    bool result = extInput->setDistanceExtent(false, distance);
    if (!result)
        return nullptr;

    Ptr<ExtrudeFeature> extrude = extrudes->add(extInput);
    if (!checkReturn(extrude))
        return nullptr;

    return extrude;
}