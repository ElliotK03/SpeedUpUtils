#include "Core/Application/ValueInput.h"
#include "Core/Geometry/Line3D.h"
#include "Core/Geometry/Vector3D.h"
#include "Core/UserInterface/Selection.h"
#include "Fusion/BRep/BRepEdge.h"
#include "Fusion/BRep/BRepFace.h"
#include "Fusion/Construction/ConstructionAxis.h"
#include "Fusion/Construction/ConstructionPlane.h"
#include "Fusion/Construction/ConstructionPlaneInput.h"
#include "Fusion/Construction/ConstructionPlanes.h"
#include "Fusion/FusionTypeDefs.h"
#include "Fusion/Sketch/SketchLine.h"
#include <Core/CoreAll.h>
#include <Fusion/FusionAll.h>
// #include <Cam/CamAll.h>
#include <map>
#include <string>
// #include <sstream>

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

bool validateMirrorPlaneOrLineSelections();
bool areLinesOrthogonal(Ptr<Line3D> line1,Ptr<Line3D> line2);
bool areLinesOrthogonal(Ptr<Vector3D> vec1,Ptr<Vector3D> vec2);
bool processPlaneOrLineInputs(Ptr<ConstructionPlane> & plane1, Ptr<ConstructionPlane> & plane2);

std::string selectionClassTypes[5] = {
    BRepEdge::classType(),
    ConstructionAxis::classType(),
    SketchLine::classType(),
    BRepFace::classType(),
    ConstructionPlane::classType()
};

Ptr<ObjectCollection> bodies;
Ptr<ConstructionPlane> * sketchPlane;
Ptr<ObjectCollection> mirrorPlanes;

Ptr<Component> rootComp;
Ptr<Application> app;
Ptr<UserInterface> ui;
Ptr<Design> design;
Ptr<UnitsManager> unitsMgr;
Ptr<ConstructionPlane> xy, yz, xz;

std::map<char, Ptr<ConstructionPlane>*> axisToPlane = {
    {'X', &yz}, {'Y', &xz}, {'Z', &xy}
};

// Global command input declarations
Ptr<StringValueCommandInput> cylinderThickness;
Ptr<StringValueCommandInput> cylinderRadius;
Ptr<DropDownCommandInput> selectedAxis;
Ptr<SelectionCommandInput> bodySelection;
Ptr<SelectionCommandInput> axisOrPlaneSelection;
Ptr<TextBoxCommandInput> _errMessage;


// Parameters with placeholder values
double sweepAngle = M_PI_2; // 180 degrees in radians (π radians)
double radius = 2.5;        // 0.1 cm = 1 mm (API uses centimeters)
double thickness = 1.0;

// Event handlers

class DoubleMirrorCommandExecuteEventHandler : public adsk::core::CommandEventHandler
{
  public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override
    {
        bool executeSuccessful = execute(rootComp);
        if ((executeSuccessful)) {
            eventArgs->executeFailed(false);
        } else { 
            eventArgs->executeFailed(true);
            eventArgs->executeFailedMessage("Unexpected failure while performing double mirror.");
        }
    }
} _doubleMirrorCommandExecute;

class DoubleMirrorCommandInputChangedHandler : public adsk::core::InputChangedEventHandler
{
  public:
    void notify(const Ptr<InputChangedEventArgs>& eventArgs) override
    {
        Ptr<CommandInput> changedInput = eventArgs->input();

        // if (changedInput->id() == "cylinderThickness") 
        // {
        //     thickness = unitsMgr->evaluateExpression(cylinderThickness->value());
        //     // ui->messageBox(std::to_string(thickness));
        // } else if (changedInput->id() == "cylinderRadius")
        // {
        //     radius = unitsMgr->evaluateExpression(cylinderRadius->value());
        // } else if (changedInput->id() == "selectedAxis") 
        // {   
        //     mirrorPlanes -> clear();
        //     mirrorPlanes -> add(xy);
        //     mirrorPlanes -> add(yz);
        //     mirrorPlanes -> add(xz);

        //     auto it = axisToPlane.find(selectedAxis->selectedItem()->name()[0]);
        //     if (it != axisToPlane.end()) {
        //         sketchPlane = it->second;
        //         mirrorPlanes->removeByItem(*sketchPlane);
        //     }
        // }
        // Verify planes/axes selection
        // validateMirrorPlaneSelections();

        // To-do? :
        // Verify intersections (if any) between the body and the planes/lines occur at boundaries only
        // And verify resultant body of double mirror operation will not interfere with other existing bodies

        return;
    }
} _doubleMirrorCommandInputChanged;

class DoubleMirrorCommandValidateInputsEventHandler : public adsk::core::ValidateInputsEventHandler
{
  public:
  void notify(const Ptr<ValidateInputsEventArgs>& eventArgs) override
  {
    _errMessage->text("");

    // std::string defUnits = unitsMgr->defaultLengthUnits();
    // if (!checkUnits(cylinderThickness->value(), unitsMgr)||!checkUnits(cylinderRadius->value(), unitsMgr)) 
    // {
    //     eventArgs->areInputsValid(false);
    //     _errMessage->text("Invalid units.");
    //     return;
    // }
    
    // if (radius <= 0 || thickness <= 0) 
    // {
    //     eventArgs->areInputsValid(false);
    //     _errMessage->text("Quantities must be positive.");
    //     return;
    // }

    if (!validateMirrorPlaneOrLineSelections()) {
        eventArgs->areInputsValid(false);
        return;
    }

    return;
  }
} _doubleMirrorCommandValidateInputs;

class DoubleMirrorCommandDestroyEventHandler : public adsk::core::CommandEventHandler
{
public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override 
    {
        adsk::terminate();
    }
} _doubleMirrorCommandDestroy;

class DoubleMirrorCommandCreatedEventHandler : public adsk::core::CommandCreatedEventHandler
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

        // cylinderThickness = inputs->addStringValueInput("cylinderThickness", "Cylinder Thickness", "10 mm");
        // cylinderRadius = inputs->addStringValueInput("cylinderRadius", "Cylinder Radius", "25 mm");
        // selectedAxis = inputs->addDropDownCommandInput("selectedAxis", "Cylinder Axis", adsk::core::TextListDropDownStyle);
        
        // selectedAxis ->listItems() ->add("X", true);
        // selectedAxis ->listItems() ->add("Y", false);
        // selectedAxis ->listItems() ->add("Z", false);

        // mirrorPlanes->add(xy);
        // mirrorPlanes->add(xz);
        // sketchPlane = &yz;

        axisOrPlaneSelection = inputs->addSelectionInput("selectionInput", "Mirror Plane or Axis", "Select plane, axis, face, edge, or sketch line");
        
        // Add multiple selection filters
        axisOrPlaneSelection->addSelectionFilter("ConstructionPlanes");
        axisOrPlaneSelection->addSelectionFilter("ConstructionLines");
        axisOrPlaneSelection->addSelectionFilter("PlanarFaces");
        axisOrPlaneSelection->addSelectionFilter("LinearEdges");
        axisOrPlaneSelection->addSelectionFilter("SketchLines");
        
        // Optional: Set to allow only one selection
        axisOrPlaneSelection->setSelectionLimits(2, 2);
        
        bodySelection = inputs->addSelectionInput("bodySelection", "Body", "Select a solid body");
        bodySelection->addSelectionFilter("SolidBodies");
        bodySelection->setSelectionLimits(1, 1);

        _errMessage = inputs->addTextBoxCommandInput("errMessage", "", "", 3, true);
        _errMessage->isReadOnly(true);

        // Connect to the command related events.
        Ptr<InputChangedEvent> inputChangedEvent = cmd->inputChanged();
        if (!inputChangedEvent)
            return;
        bool isOk = inputChangedEvent->add(&_doubleMirrorCommandInputChanged);
        if (!isOk)
            return;

        Ptr<ValidateInputsEvent> validateInputsEvent = cmd->validateInputs();
        if (!validateInputsEvent)
            return;
        isOk = validateInputsEvent->add(&_doubleMirrorCommandValidateInputs);
        if (!isOk)
            return;

        Ptr<CommandEvent> executeEvent = cmd->execute();
        if (!executeEvent)
            return;
        isOk = executeEvent->add(&_doubleMirrorCommandExecute);
        if (!isOk)
            return;

        Ptr<CommandEvent> destroyEvent = cmd->destroy();
        if (!destroyEvent)
            return;

        isOk = destroyEvent->add(&_doubleMirrorCommandDestroy);
        if (!isOk)
            return;

    }
} _doubleMirrorCommandCreated;

extern "C" XI_EXPORT bool run(const char *context)
{
    bodies = ObjectCollection::create();
    if (!bodies)
        return false;

    mirrorPlanes = ObjectCollection::create();
    if (!mirrorPlanes)
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
    if (!checkReturn(unitsMgr))
        return false;

    rootComp = design->rootComponent();
    if (!checkReturn(rootComp))
        return false;

    xy = rootComp->xYConstructionPlane();
    if (!xy)
        return false;
    
    yz = rootComp->yZConstructionPlane();
    if (!yz)
        return false;

    xz = rootComp->xZConstructionPlane();
    if (!xz)
        return false;

    std::string idString = "DoubleMirrorTool";

    // Create a command definition and add a button to the CREATE panel.
    Ptr<CommandDefinition> cmdDef = ui->commandDefinitions()->itemById(idString);
    if (!cmdDef)
    {
        cmdDef = ui->commandDefinitions()->addButtonDefinition(
            idString, "Double-mirror Command", "Turns a quarter-body into a full body, provided your intended full body is symmetrical about 2 axes");
        if (!checkReturn(cmdDef))
            return false;
    }

    Ptr<CommandCreatedEvent> commandCreatedEvent = cmdDef->commandCreated();
    if (!checkReturn(commandCreatedEvent))
        return false;
    bool isOk = commandCreatedEvent->add(&_doubleMirrorCommandCreated);
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

    Ptr<Sketch> sketch = sketches->add(*sketchPlane);
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
    if (!checkReturn(mirrorFeatureNew))
        return nullptr;
    return mirrorFeatureNew;
}

bool execute(Ptr<Component> rootComp) {

    if (bodySelection->selectionCount() != 1) {
        ui->messageBox("no body selected");
        return false;
    }

    Ptr<BRepBody> selectedBody = bodySelection->selection(0)->entity();

    // There is always one body selected at this point
    bodies->add(selectedBody);
    if (!checkReturn(bodies))
        return false;

    // Ptr<ConstructionPlane>  plane1 = mirrorPlanes->item(0), 
    //                         plane2 = mirrorPlanes->item(1);

    Ptr<ConstructionPlane> plane1, plane2;

    bool isOk = processPlaneOrLineInputs(plane1, plane2);
    if (!isOk) {
        ui->messageBox("Unable to process input axes/planes selection");
        return false;
    }

    Ptr<MirrorFeature> newMirrorFeature = doubleMirror(bodies, plane1, plane2);

    if (!checkReturn(newMirrorFeature)) {
        ui->messageBox("Double mirror failed");
        return false;
    }

    return true;
}

bool validateMirrorPlaneOrLineSelections() {
    
    if (axisOrPlaneSelection->selectionCount() != 2) return false;

    auto entity1 = axisOrPlaneSelection->selection(0)->entity(), 
         entity2 = axisOrPlaneSelection->selection(1)->entity();

    if (entity1->objectType() != entity2->objectType())
    {
        _errMessage->text("Please select the same type for mirror guides (both planes or both lines)");
        return false;
    }

    //check lines are orthogonal
    Ptr<BRepEdge> edge1 = entity1;
    Ptr<BRepEdge> edge2 = entity2;

    if (checkReturn(edge1)&&checkReturn(edge2)) {
        Ptr<Line3D> line1 = edge1->geometry();
        Ptr<Line3D> line2 = edge2->geometry();
        if (!checkReturn(line1) || !checkReturn(line2)) {
            _errMessage->text("object is not a straight line");
            return false;
        }
        else {
            return areLinesOrthogonal(line1, line2);
        }
    }
    
    Ptr<ConstructionAxis> axis1 = entity1;
    Ptr<ConstructionAxis> axis2 = entity2;

    if (checkReturn(axis1)&&checkReturn(axis2)) {
        Ptr<Vector3D> line1dir = axis1->geometry()->direction();
        Ptr<Vector3D> line2dir = axis2->geometry()->direction();
        if (!checkReturn(line1dir) || !checkReturn(line2dir)) {
            _errMessage->text("Unable to resolve directions of construction axes");
            return false;
        }
        else {
            return areLinesOrthogonal(line1dir, line2dir);
        }
    }
    
    Ptr<SketchLine> sketchLine1 = entity1;
    Ptr<SketchLine> sketchLine2 = entity2;
    
    if (checkReturn(sketchLine1)&&checkReturn(sketchLine2)) {
        Ptr<Line3D> line1 = sketchLine1->geometry();
        Ptr<Line3D> line2 = sketchLine2->geometry();
        
        if (!checkReturn(line1) || !checkReturn(line2)) {
            _errMessage->text("object is not a sketch line");
            return false;
        }
        
        return areLinesOrthogonal(line1, line2);
    }
    
    // Planar face is assumed
    Ptr<BRepFace> face1 = entity1;
    Ptr<BRepFace> face2 = entity2;

    if (checkReturn(face1) && checkReturn(face2)) {
        Ptr<Point3D> point1 = face1->pointOnFace();
        Ptr<Point3D> point2 = face2->pointOnFace();

        if (!checkReturn(point1)||!checkReturn(point2))
        {
            _errMessage->text("Unable to obtain point(s) on face(s)");
            return false;
        }

        Ptr<Vector3D> normal1;
        Ptr<Vector3D> normal2;

        auto isOk = face1->geometry()->evaluator()->getNormalAtPoint(point1, normal1);
        if (!isOk) {
            _errMessage->text("Cannot obtain normal vector of face 1.");
            return false;
        }
        isOk = face2->geometry()->evaluator()->getNormalAtPoint(point2, normal2);
        if (!isOk) {
            _errMessage->text("Cannot obtain normal vector of face 1.");
            return false;
        }

        return areLinesOrthogonal(normal1, normal2);
    }

    Ptr<ConstructionPlane> cPlane1 = entity1;
    Ptr<ConstructionPlane> cPlane2 = entity2;

    if (checkReturn(cPlane1)&&checkReturn(cPlane2))
    {
        Ptr<Plane> plane1 = cPlane1->geometry();
        Ptr<Plane> plane2 = cPlane2->geometry();
        if (plane1->isPerpendicularToPlane(plane2)) {
            return true;
        } else {
            _errMessage->text("Planes are not perpendicular");
            return false;
        }
    }
    {
        _errMessage->text("Invalid object type (bad filter) for selections");
        return false;
    }
}

bool areLinesOrthogonal(Ptr<Line3D> line1,Ptr<Line3D> line2) {
    
    Ptr<Point3D> l1start, l1end, l2start, l2end;
    line1->getData(l1start, l1end);
    line2->getData(l2start, l2end);
    
    Ptr<Vector3D> vec1 = l1start->vectorTo(l1end), 
                  vec2 = l2start->vectorTo(l2end);

    return areLinesOrthogonal(vec1, vec2);
}

bool areLinesOrthogonal(Ptr<Vector3D> vec1,Ptr<Vector3D> vec2) {

    bool result = (vec1->isPerpendicularTo(vec2));

    if (!result) _errMessage->text("Lines are not orthogonal");

    return result;
}

int findIndex() {
    std::string selectedObjectType = axisOrPlaneSelection->selection(0)->entity()->objectType();
    int i;

    for (i=0; i<5; i++) {
        if (selectedObjectType == selectionClassTypes[i])
            return i;
    }
    return -1;
}

bool constructPlanesFromLines(Ptr<ConstructionPlanes> constructionPlanes, Ptr<Base> linearEntity1, Ptr<Base> linearEntity2, Ptr<ConstructionPlane> &plane1, Ptr<ConstructionPlane> &plane2) {

    auto angle = ValueInput::createByReal(M_PI_2);
    auto constructionPlaneInput = constructionPlanes->createInput();
    
    if (!checkReturn(angle) || !checkReturn(constructionPlaneInput))
        return false;
    
    constructionPlaneInput->setByTwoEdges(linearEntity1, linearEntity2);
    Ptr<ConstructionPlane> refPlane = constructionPlanes->add(constructionPlaneInput);
            
    if (!checkReturn(refPlane)) {
        return false;
    }

    constructionPlaneInput->setByAngle(linearEntity1,angle,refPlane);
    plane1 = constructionPlanes->add(constructionPlaneInput);

    constructionPlaneInput->setByAngle(linearEntity2,angle,refPlane);
    plane2 = constructionPlanes->add(constructionPlaneInput);

    if (!checkReturn(plane1) || !checkReturn(plane2)) {
        ui->messageBox("Unable to construct planes from lines");
        return false;
    }

    return true;
}

bool processPlaneOrLineInputs(Ptr<ConstructionPlane> &plane1, Ptr<ConstructionPlane> &plane2) {

    Ptr<Base> entity1 = axisOrPlaneSelection->selection(0)->entity();
    Ptr<Base> entity2 = axisOrPlaneSelection->selection(1)->entity();
   
    Ptr<BRepBody> selectedBody = bodySelection->selection(0)->entity();
    if (!checkReturn(selectedBody))
        return false;

    Ptr<ConstructionPlanes> cPlanes = selectedBody->parentComponent()->constructionPlanes();

    auto offset = ValueInput::createByReal(0);

    Ptr<Component> parentComp = selectedBody->parentComponent();
    if (!parentComp) {
        // Body is in root component
        parentComp = rootComp;
    }
    std::string selectedObjectType = entity1->objectType();
    
    if (selectedObjectType != entity2->objectType()) {
        ui->messageBox("Selections are not of the same type");
        return false;
    }
    
    int index = findIndex();

    switch (index) {
        case 0: 
        {
            Ptr<BRepEdge> edge1 = entity1,
                                  edge2 = entity2;
                                  
            bool isOk = constructPlanesFromLines(cPlanes, edge1, edge2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
            // break;
        }
        case 1:
        {    
            Ptr<ConstructionAxis> axis1 = entity1,
                                  axis2 = entity2;

            bool isOk = constructPlanesFromLines(cPlanes, axis1, axis2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
            // break;
        }
        case 2: 
        {    
            Ptr<SketchLine> line1 = entity1, 
                            line2 = entity2;

            bool isOk = constructPlanesFromLines(cPlanes, line1, line2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
            // break;
        }
        case 3: 
        {
            Ptr<BRepFace> face1 = entity1, 
                          face2 = entity2;

            auto constructionPlaneInput = cPlanes->createInput();

            // Ptr<Point3D> centroid1 = face1->centroid();
            constructionPlaneInput->setByOffset(face1, offset);
            plane1 = cPlanes->add(constructionPlaneInput);
            
            constructionPlaneInput->setByOffset(face2, offset);
            plane2 = cPlanes->add(constructionPlaneInput);
            if (!checkReturn(plane1) || !checkReturn(plane2)) return false;

            return true;
        }
        
        case 4: 
        {
            Ptr<ConstructionPlane> cplane1 = entity1, 
                                   cplane2 = entity2;

            plane1 = cplane1;
            plane2 = cplane2;

            return true;
            // break;
        }
    }

    return false;
}