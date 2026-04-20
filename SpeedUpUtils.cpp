#include <Core/CoreAll.h>
#include <Fusion/FusionAll.h>

#include <string>
#include <array>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace adsk::core;
using namespace adsk::fusion;

static Ptr<MirrorFeature> doubleMirror(Ptr<ObjectCollection>, Ptr<ConstructionPlane>, Ptr<ConstructionPlane>);
static bool checkReturn(Ptr<Base> returnObj);
static bool execute();

static bool validateMirrorPlaneOrLineSelections();
static bool areLinesOrthogonal(Ptr<Vector3D> vec1,Ptr<Vector3D> vec2);
static bool areLinesOrthogonal(Ptr<Line3D> line1,Ptr<Line3D> line2);
static bool processPlaneOrLineInputs(Ptr<ConstructionPlane> & plane1, Ptr<ConstructionPlane> & plane2);

static const std::array<const char*, 5> selectionClassTypes = {
    BRepEdge::classType(),
    ConstructionAxis::classType(),
    SketchLine::classType(),
    BRepFace::classType(),
    ConstructionPlane::classType()
};

Ptr<ObjectCollection> bodies;

Ptr<Component> rootComp, activeComp;
Ptr<Application> app;
Ptr<UserInterface> ui;
Ptr<Design> design;
Ptr<UnitsManager> unitsMgr;

// Global command input declarations
Ptr<SelectionCommandInput> bodySelection;
Ptr<SelectionCommandInput> axisOrPlaneSelection;
Ptr<TextBoxCommandInput> cmdBoxErrMessage;

// Event handlers

class DoubleMirrorCommandExecuteEventHandler : public adsk::core::CommandEventHandler
{
  public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override
    {
        bool executeSuccessful = execute();
        if ((executeSuccessful)) {
            // eventArgs->executeFailed(false);
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

        return;
    }
} _doubleMirrorCommandInputChanged;

class DoubleMirrorCommandValidateInputsEventHandler : public adsk::core::ValidateInputsEventHandler
{
  public:
  void notify(const Ptr<ValidateInputsEventArgs>& eventArgs) override
  {
    cmdBoxErrMessage->text("");

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

        activeComp = des->activeComponent();

        Ptr<Command> cmd = eventArgs->command();
        cmd->isExecutedWhenPreEmpted(false);
        Ptr<CommandInputs> inputs = cmd->commandInputs();
        if (!checkReturn(inputs))
            return;

        bodySelection = inputs->addSelectionInput("bodySelection", "Body", "Select a solid body");
        bodySelection->addSelectionFilter("SolidBodies");
        bodySelection->setSelectionLimits(1, 0);

        axisOrPlaneSelection = inputs->addSelectionInput("planeOrAxisSelection", "Mirror Plane or Axis", "Select plane, axis, face, edge, or sketch line");
        
        // Add multiple selection filters
        axisOrPlaneSelection->addSelectionFilter("ConstructionPlanes");
        axisOrPlaneSelection->addSelectionFilter("ConstructionLines");
        axisOrPlaneSelection->addSelectionFilter("PlanarFaces");
        axisOrPlaneSelection->addSelectionFilter("LinearEdges");
        axisOrPlaneSelection->addSelectionFilter("SketchLines");
        
        axisOrPlaneSelection->setSelectionLimits(2, 2);

        cmdBoxErrMessage = inputs->addTextBoxCommandInput("errMessage", "", "", 3, true);
        cmdBoxErrMessage->isReadOnly(true);

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

    app = Application::get();
    if (!app)
        return false;

    ui = app->userInterface();
    if (!ui)
        return false;

    Ptr<Documents> documents = app->documents();
    if (!documents)
        return false;

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

    std::string idString = "DoubleMirror";

    // Create a command definition and add a button to the CREATE panel.
    Ptr<CommandDefinition> cmdDef = ui->commandDefinitions()->itemById(idString);
    if (!cmdDef)
    {
        cmdDef = ui->commandDefinitions()->addButtonDefinition(
            idString, "Double Mirror", "Build complete symmetric parts from quarter sections - mirrors twice across perpendicular axes");
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

static bool checkReturn(Ptr<Base> returnObj)
{
    if (returnObj)
        return true;
    else if (app && ui)
    {
        std::string errDesc;
        app->getLastError(&errDesc);
        if (!cmdBoxErrMessage)
            ui->statusMessage(errDesc);
        else
            cmdBoxErrMessage->text(errDesc);
        return false;
    }
    else
        return false;
}

static Ptr<MirrorFeature> doubleMirror(Ptr<ObjectCollection> bodies, Ptr<ConstructionPlane> plane1, Ptr<ConstructionPlane> plane2) {
    Ptr<MirrorFeatureInput> mirrorInput = activeComp->features()->mirrorFeatures()->createInput(bodies, plane1);
    if (!checkReturn(mirrorInput))
        return nullptr;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeature = activeComp->features()->mirrorFeatures()->add(mirrorInput);
    if (!checkReturn(mirrorFeature))
        return nullptr;

    // Update bodies collection to include the mirrored result
    bodies->clear();

    for (int i = 0; i < mirrorFeature->bodies()->count(); i++)
    {
        bodies->add(mirrorFeature->bodies()->item(i));
    }

    mirrorInput = activeComp->features()->mirrorFeatures()->createInput(bodies, plane2);
    if (!checkReturn(mirrorInput))
        return nullptr;

    // Set mirror operation to combine bodies
    mirrorInput->isCombine(true);

    // Create mirror feature
    Ptr<MirrorFeature> mirrorFeatureNew = activeComp->features()->mirrorFeatures()->add(mirrorInput);
    if (!checkReturn(mirrorFeatureNew))
        return nullptr;
    return mirrorFeatureNew;
}

static bool execute() {

    bodies->clear();

    if (bodySelection->selectionCount() < 1) {
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

static bool validateMirrorPlaneOrLineSelections() {
    
    if (axisOrPlaneSelection->selectionCount() != 2) return false;

    auto entity1 = axisOrPlaneSelection->selection(0)->entity(), 
         entity2 = axisOrPlaneSelection->selection(1)->entity();

    if (entity1->objectType() != entity2->objectType())
    {
        cmdBoxErrMessage->text("Please select the same type for mirror guides (both planes or both lines)");
        return false;
    }

    //check lines are orthogonal
    Ptr<BRepEdge> edge1 = entity1;
    Ptr<BRepEdge> edge2 = entity2;

    if (checkReturn(edge1)&&checkReturn(edge2)) {
        Ptr<Line3D> line1 = edge1->geometry();
        Ptr<Line3D> line2 = edge2->geometry();
        if (!checkReturn(line1) || !checkReturn(line2)) {
            cmdBoxErrMessage->text("object is not a straight line");
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
            cmdBoxErrMessage->text("Unable to resolve directions of construction axes");
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
            cmdBoxErrMessage->text("object is not a sketch line");
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
            cmdBoxErrMessage->text("Unable to obtain point(s) on face(s)");
            return false;
        }

        Ptr<Vector3D> normal1;
        Ptr<Vector3D> normal2;

        auto isOk = face1->geometry()->evaluator()->getNormalAtPoint(point1, normal1);
        if (!isOk) {
            cmdBoxErrMessage->text("Cannot obtain normal vector of face 1.");
            return false;
        }
        isOk = face2->geometry()->evaluator()->getNormalAtPoint(point2, normal2);
        if (!isOk) {
            cmdBoxErrMessage->text("Cannot obtain normal vector of face 2.");
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
            cmdBoxErrMessage->text("Planes are not perpendicular");
            return false;
        }
    }

    // trailing block if all the typecasting attempts fail
    {
        cmdBoxErrMessage->text("Invalid object type (bad filter) for selections");
        return false;
    }
}

static bool areLinesOrthogonal(Ptr<Vector3D> vec1,Ptr<Vector3D> vec2) {

    if (!checkReturn(vec1) || !checkReturn(vec2)) {
        cmdBoxErrMessage->text("Nonexistent vector(s)");
        return false;
    }

    if (vec1->length() * vec2->length() <= 0) {
        cmdBoxErrMessage->text("Degenerate vector(s)");
        return false;
    }

    bool result = (vec1->isPerpendicularTo(vec2));
    if (!result) cmdBoxErrMessage->text("Lines are not orthogonal");

    return result;
}

static bool areLinesOrthogonal(Ptr<Line3D> line1,Ptr<Line3D> line2) {
    
    Ptr<Point3D> l1start, l1end, l2start, l2end;
    line1->getData(l1start, l1end);
    line2->getData(l2start, l2end);
    
    Ptr<Vector3D> vec1 = l1start->vectorTo(l1end), 
                  vec2 = l2start->vectorTo(l2end);

    return areLinesOrthogonal(vec1, vec2);
}

static int findIndex() {
    std::string selectedObjectType = axisOrPlaneSelection->selection(0)->entity()->objectType();
    int i;

    for (i=0; i<5; i++) {
        if (selectedObjectType == selectionClassTypes[i])
            return i;
    }
    return -1;
}

static bool constructPlanesFromLines(Ptr<ConstructionPlanes> constructionPlanes, Ptr<Base> linearEntity1, Ptr<Base> linearEntity2, Ptr<ConstructionPlane> &plane1, Ptr<ConstructionPlane> &plane2) {

    auto angle = ValueInput::createByReal(M_PI/2.0);
    auto constructionPlaneInput = constructionPlanes->createInput();
    
    if (!checkReturn(angle) || !checkReturn(constructionPlaneInput))
        return false;
    
    constructionPlaneInput->setByTwoEdges(linearEntity1, linearEntity2);
    Ptr<ConstructionPlane> refPlane = constructionPlanes->add(constructionPlaneInput);
            
    if (!checkReturn(refPlane)) {
        return false;
    }
    
    auto input1 = constructionPlanes->createInput();
    input1->setByAngle(linearEntity1,angle,refPlane);
    plane1 = constructionPlanes->add(input1);

    auto input2 = constructionPlanes->createInput();
    input2->setByAngle(linearEntity2,angle,refPlane);
    plane2 = constructionPlanes->add(input2);

    if (!checkReturn(plane1) || !checkReturn(plane2)) {
        ui->messageBox("Unable to construct planes from lines");
        return false;
    }

    return true;
}

static bool processPlaneOrLineInputs(Ptr<ConstructionPlane> &plane1, Ptr<ConstructionPlane> &plane2) {
    if (!axisOrPlaneSelection || !bodySelection) { 
        cmdBoxErrMessage->text("UI not initialized");
        return false; 
    }
    if (axisOrPlaneSelection->selectionCount() != 2) { 
        cmdBoxErrMessage->text("Impossible error: selection count is not 2");
        return false; 
    }

    Ptr<Base> entity1 = axisOrPlaneSelection->selection(0)->entity();
    Ptr<Base> entity2 = axisOrPlaneSelection->selection(1)->entity();
   
    Ptr<BRepBody> selectedBody = bodySelection->selection(0)->entity();
    if (!checkReturn(selectedBody))
        return false;

    Ptr<ValueInput> offset = ValueInput::createByReal(0);

    auto offset = ValueInput::createByReal(0);

    Ptr<Component> parentComp = selectedBody->parentComponent();
    if (!parentComp) {
        // Body is in root component
        parentComp = rootComp;
    } else if (parentComp != activeComp)
    {
        cmdBoxErrMessage->text("Working across components is not supported.");
        return false;
    }

    Ptr<ConstructionPlanes> cPlanes = parentComp->constructionPlanes();
    if (!checkReturn(cPlanes)) {
        return false;
    }

    std::string selectedObjectType = entity1->objectType();
    
    if (selectedObjectType != entity2->objectType()) {
        cmdBoxErrMessage->text("Selections are not of the same type");
        return false;
    }
    
    int index = findIndex();
    if (index < 0) {
        cmdBoxErrMessage->text("Unsupported selection type");
        return false;
    }
    
    switch (index) {
        case 0: //BRepEdge
        {
            Ptr<BRepEdge> edge1 = entity1,
                          edge2 = entity2;
                                  
            bool isOk = constructPlanesFromLines(cPlanes, edge1, edge2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
        }
        case 1: //ConstructionAxis
        {    
            Ptr<ConstructionAxis> axis1 = entity1,
                                  axis2 = entity2;

            bool isOk = constructPlanesFromLines(cPlanes, axis1, axis2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
        }
        case 2: //SketchLine
        {    
            Ptr<SketchLine> line1 = entity1, 
                            line2 = entity2;

            bool isOk = constructPlanesFromLines(cPlanes, line1, line2, plane1, plane2);

            if (!isOk || !checkReturn(plane1) || !checkReturn(plane2)) return false;
            else return true;
        }
        case 3: //BRepFace
        {
            Ptr<BRepFace> face1 = entity1, 
                          face2 = entity2;

            auto constructionPlaneInput = cPlanes->createInput();

            constructionPlaneInput->setByOffset(face1, offset);
            plane1 = cPlanes->add(constructionPlaneInput);
            
            constructionPlaneInput = cPlanes->createInput();
            constructionPlaneInput->setByOffset(face2, offset);
            plane2 = cPlanes->add(constructionPlaneInput);
            if (!checkReturn(plane1) || !checkReturn(plane2)) return false;

            return true;
        }
        
        case 4: //ConstructionPlane
        {
            Ptr<ConstructionPlane> cplane1 = entity1, 
                                   cplane2 = entity2;

            plane1 = cplane1;
            plane2 = cplane2;

            return true;
        }
    }

    return false;
}