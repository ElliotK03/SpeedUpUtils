
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

Ptr<ExtrudeFeature> extrudeCreatedSketch(Ptr<Component>, Ptr<Sketch>);
bool checkReturn(Ptr<Base>);

Ptr<Application> app;
Ptr<UserInterface> ui;
Ptr<ToolbarPanelList> toolbarPanelList;
Ptr<ToolbarControlList> toolbarControlList_var;

// Parameters with placeholder values
double sweepAngle = M_PI_2; // 180 degrees in radians (π radians)
double radius = 2.5;        // 0.1 cm = 1 mm (API uses centimeters)
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

    // Create center point at origin
    Ptr<Point3D> centerPoint = Point3D::create(0, 0, 0);
    if (!centerPoint)
        return false;
    
    // y = radius cm, because the y-axis is projected onto z-axis on the sketch (which is based on the xz-plane)
    Ptr<Point3D> endPoint = Point3D::create(0, radius, 0);
    if (!endPoint)
        return false;

    // Line 1
    sketchLines->addByTwoPoints(startPoint, centerPoint);
    // Line 2
    sketchLines->addByTwoPoints(endPoint, centerPoint);

    Ptr<SketchArcs> sketchArcs = sketchCurves->sketchArcs();
    if (!sketchArcs)
        return false;

    Ptr<SketchArc> arc = sketchArcs->addByCenterStartSweep(centerPoint, startPoint, sweepAngle);
    if (!arc)
        return false;

    Ptr<ExtrudeFeature> extrusion = extrudeCreatedSketch(rootComp, sketch);
    if (!extrusion)
        return false;

    // Since the sketch of the quarter circle is created with its straight edges both parallel with two of the origin axes, namely
    // the x-axis and the z-axis.
    // The extrusion is orthogonal to these 2 axes. To create a cyllinder, select the planes normal to both of the axes.
    // In this case, the XY-plane and the YZ-plane.

    Ptr<ConstructionPlane> xy = rootComp->xYConstructionPlane();
    if (!xy)
        return false;
    
    Ptr<ConstructionPlane> yz = rootComp->yZConstructionPlane();
    if (!yz)
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

    // Update bodies collection to include the mirrored result
    bodies->clear();
    for (int i = 0; i < mirrorFeature->bodies()->count(); i++)
    {
        bodies->add(mirrorFeature->bodies()->item(i));
    }

    mirrorInput = rootComp->features()->mirrorFeatures()->createInput(bodies, yz);
    if (!checkReturn(mirrorInput))
        return false;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeatureNew = rootComp->features()->mirrorFeatures()->add(mirrorInput);
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

Ptr<ExtrudeFeature> extrudeCreatedSketch(Ptr<Component> component, Ptr<Sketch> sketch)
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

