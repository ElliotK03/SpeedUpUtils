
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

Ptr<Sketch> createSketch(Ptr<Component>);
Ptr<ExtrudeFeature> extrudeCreatedSketch(Ptr<Component>, Ptr<Sketch>);
Ptr<MirrorFeature> doubleMirror(Ptr<ObjectCollection>, Ptr<ConstructionPlane>, Ptr<ConstructionPlane>);
bool checkUnits(std::string, Ptr<UnitsManager>);
bool checkReturn(Ptr<Base>);
bool execute(Ptr<Component>);

Ptr<ObjectCollection> bodies;


Ptr<Component> rootComp;
Ptr<Application> app;
Ptr<UserInterface> ui;
Ptr<Design> design;
Ptr<UnitsManager> unitsMgr;

// Global command input declarations
Ptr<StringValueCommandInput> cylinderThickness;
Ptr<StringValueCommandInput> cylinderRadius;
Ptr<TextBoxCommandInput> _errMessage;


// Parameters with placeholder values
double sweepAngle = M_PI_2; // 180 degrees in radians (π radians)
double radius = 2.5;        // 0.1 cm = 1 mm (API uses centimeters)
double thickness = 1.0;

// Event handlers

class CylinderCommandExecuteEventHandler : public adsk::core::CommandEventHandler
{
  public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override
    {
        rootComp = design->rootComponent();
        if (!checkReturn(rootComp))
            return;
        bool executeSuccessful = execute(rootComp);
        if ((executeSuccessful)) {
            eventArgs->executeFailed(false);
        } else { 
            eventArgs->executeFailed(true);
            eventArgs->executeFailedMessage("Unexpected failure while constructing the cylinder.");
        }
    }
} _cylinderCommandExecute;

class CylinderCommandInputChangedHandler : public adsk::core::InputChangedEventHandler
{
  public:
    void notify(const Ptr<InputChangedEventArgs>& eventArgs) override
    {
        Ptr<CommandInput> changedInput = eventArgs->input();

        if (changedInput->id() == "cylinderThickness") 
        {
            thickness = unitsMgr->evaluateExpression(cylinderThickness->value());
            // ui->messageBox(std::to_string(thickness));
        } else if (changedInput->id() == "cylinderRadius")
        {
            radius = unitsMgr->evaluateExpression(cylinderRadius->value());
        }

        return;
    }
} _cylinderCommandInputChanged;

class CylinderCommandValidateInputsEventHandler : public adsk::core::ValidateInputsEventHandler
{
  public:
  void notify(const Ptr<ValidateInputsEventArgs>& eventArgs) override
  {
    _errMessage->text("");

    std::string defUnits = unitsMgr->defaultLengthUnits();
    if (!checkUnits(cylinderThickness->value(), unitsMgr)||!checkUnits(cylinderRadius->value(), unitsMgr)) 
    {
        eventArgs->areInputsValid(false);
        _errMessage->text("Invalid units.");
        return;
    }
    
    if (radius <= 0 || thickness <= 0) 
    {
        eventArgs->areInputsValid(false);
        _errMessage->text("Quantities must be positive.");
        return;
    }

    return;
  }
} _cylinderCommandValidateInputs;

class CylinderCommandDestroyEventHandler : public adsk::core::CommandEventHandler
{
public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override 
    {
        adsk::terminate();
    }
} _cylinderCommandDestroy;

class CylinderCommandCreatedEventHandler : public adsk::core::CommandCreatedEventHandler
{
public:
    void notify(const Ptr<CommandCreatedEventArgs>& eventArgs) override
    {
        Ptr<Design> des = app->activeProduct();
        if (!checkReturn(des))
        {
            ui->messageBox("The DESIGN workspace must be active when running this script.");
            return;
        }

        Ptr<Command> cmd = eventArgs->command();
        cmd->isExecutedWhenPreEmpted(false);
        Ptr<CommandInputs> inputs = cmd->commandInputs();
        if (!checkReturn(inputs))
            return;

        cylinderThickness = inputs->addStringValueInput("cylinderThickness", "Cylinder Thickness", "10 mm");
        cylinderRadius = inputs->addStringValueInput("cylinderRadius", "Cylinder Radius", "25 mm");
        _errMessage = inputs->addTextBoxCommandInput("errMessage", "", "", 2, true);
        _errMessage->isReadOnly(true);

        // Connect to the command related events.
        Ptr<InputChangedEvent> inputChangedEvent = cmd->inputChanged();
        if (!inputChangedEvent)
            return;
        bool isOk = inputChangedEvent->add(&_cylinderCommandInputChanged);
        if (!isOk)
            return;

        Ptr<ValidateInputsEvent> validateInputsEvent = cmd->validateInputs();
        if (!validateInputsEvent)
            return;
        isOk = validateInputsEvent->add(&_cylinderCommandValidateInputs);
        if (!isOk)
            return;

        Ptr<CommandEvent> executeEvent = cmd->execute();
        if (!executeEvent)
            return;
        isOk = executeEvent->add(&_cylinderCommandExecute);
        if (!isOk)
            return;

        Ptr<CommandEvent> destroyEvent = cmd->destroy();
        if (!destroyEvent)
            return;

        isOk = destroyEvent->add(&_cylinderCommandDestroy);
        if (!isOk)
            return;

    }
} _cylinderCommandCreated;

extern "C" XI_EXPORT bool run(const char *context)
{
    bodies = ObjectCollection::create();
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

    // Ptr<Document> doc = documents->add(DocumentTypes::FusionDesignDocumentType);
    // if (!doc)
    //     return false;

    Ptr<Product> product = app->activeProduct();
    if (!product)
        return false;

    design = product;
    if (!design)
        return false;

    unitsMgr = design->unitsManager();

    std::string idString = "DoubleMirrorTool";

    // Create a command definition and add a button to the CREATE panel.
    Ptr<CommandDefinition> cmdDef = ui->commandDefinitions()->itemById(idString);
    if (!cmdDef)
    {
        cmdDef = ui->commandDefinitions()->addButtonDefinition(
            idString, "Double-mirror cylinder", "Creates a cylinder from a sketch containing a quarter circle");
        if (!checkReturn(cmdDef))
            return false;
    }

    Ptr<CommandCreatedEvent> commandCreatedEvent = cmdDef->commandCreated();
    if (!checkReturn(commandCreatedEvent))
        return false;
    bool isOk = commandCreatedEvent->add(&_cylinderCommandCreated);
    if (!isOk)
        return false;

    isOk = cmdDef->execute();
    if (!isOk)
        return false;

    adsk::autoTerminate(false);

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
        if (!_errMessage)
            ui->statusMessage(errDesc);
        else
            _errMessage->text(errDesc);
        return false;
    }
    else
        return false;
}

bool checkUnits(std::string input, Ptr<UnitsManager> unitsMgr) {
    double realValue;
    if (!checkReturn(unitsMgr))
        return false;

    realValue = unitsMgr->evaluateExpression(input, unitsMgr->defaultLengthUnits());
    if (app->getLastError())
    {
        // Invalid expression so display an error and set the flag to allow them
        // to enter a value again.
        // 
        std::string errDesc = input + " is not a valid length expression.";

        if (!_errMessage)
            ui->statusMessage(errDesc);
        else
            _errMessage->text(errDesc);
        
        return false;
    }
    else
        return true;
}


Ptr<Sketch> createSketch(Ptr<Component> rootComp) {
    Ptr<Sketches> sketches = rootComp->sketches();
    if (!sketches)
        return nullptr;
    Ptr<ConstructionPlane> xz = rootComp->xZConstructionPlane();
    if (!xz)
        return nullptr;

    Ptr<Sketch> sketch = sketches->add(xz);
    if (!sketch)
        return nullptr;

    Ptr<SketchCurves> sketchCurves = sketch->sketchCurves();
    if (!sketchCurves)
        return nullptr;

    Ptr<SketchLines> sketchLines = sketchCurves->sketchLines();
    if (!sketchLines)
        return nullptr;

    Ptr<Point3D> startPoint = Point3D::create(radius, 0, 0);
    if (!startPoint)
        return nullptr;

    // Create center point at origin
    Ptr<Point3D> centerPoint = Point3D::create(0, 0, 0);
    if (!centerPoint)
        return nullptr;
    
    // y = radius cm, because the y-axis is projected onto z-axis on the sketch (which is based on the xz-plane)
    Ptr<Point3D> endPoint = Point3D::create(0, radius, 0);
    if (!endPoint)
        return nullptr;

    // Line 1
    sketchLines->addByTwoPoints(startPoint, centerPoint);
    // Line 2
    sketchLines->addByTwoPoints(endPoint, centerPoint);

    Ptr<SketchArcs> sketchArcs = sketchCurves->sketchArcs();
    if (!sketchArcs)
        return nullptr;

    Ptr<SketchArc> arc = sketchArcs->addByCenterStartSweep(centerPoint, startPoint, sweepAngle);
    if (!arc)
        return nullptr;

    return sketch;
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

Ptr<MirrorFeature> doubleMirror(Ptr<ObjectCollection> bodies, Ptr<ConstructionPlane> plane1, Ptr<ConstructionPlane> plane2) {
    Ptr<MirrorFeatureInput> mirrorInput = rootComp->features()->mirrorFeatures()->createInput(bodies, plane1);
    if (!checkReturn(mirrorInput))
        return nullptr;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeature = rootComp->features()->mirrorFeatures()->add(mirrorInput);
    if (!checkReturn(mirrorFeature))
        return nullptr;

    // Update bodies collection to include the mirrored result
    bodies->clear();
    for (int i = 0; i < mirrorFeature->bodies()->count(); i++)
    {
        bodies->add(mirrorFeature->bodies()->item(i));
    }

    mirrorInput = rootComp->features()->mirrorFeatures()->createInput(bodies, plane2);
    if (!checkReturn(mirrorInput))
        return nullptr;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeatureNew = rootComp->features()->mirrorFeatures()->add(mirrorInput);
    if (!checkReturn(mirrorFeature))
        return nullptr;
    return mirrorFeatureNew;
}

bool execute(Ptr<Component> rootComp) {
    // Create sketch
    Ptr<Sketch> sketch = createSketch(rootComp);
    if (!checkReturn(sketch))
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

    Ptr<MirrorFeature> newMirrorFeature = doubleMirror(bodies, xy, yz);

    return true;
}
