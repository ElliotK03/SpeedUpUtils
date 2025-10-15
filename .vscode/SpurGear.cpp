/**************************************************************************************
  Author-Brian Ekins
  Description-Creates a spur gear component.

  AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. AUTODESK SPECIFICALLY
  DISCLAIMS ANY IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.
  AUTODESK, INC. DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
  UNINTERRUPTED OR ERROR FREE.
***************************************************************************************/

#include <Core/Utils.h>
#include <Core/Application/Application.h>
#include <Core/Application/Product.h>
#include <Core/Application/ValueInput.h>
#include <Core/Application/UnitsManager.h>
#include <Core/Application/ObjectCollection.h>
#include <Core/Geometry/Point3D.h>
#include <Core/Geometry/Matrix3D.h>
#include <Core/Geometry/Surface.h>
#include <Core/Geometry/Curve3D.h>
#include <Core/Geometry/Line3D.h>
#include <Core/UserInterface/UserInterface.h>
#include <Core/UserInterface/CommandCreatedEventHandler.h>
#include <Core/UserInterface/CommandCreatedEvent.h>
#include <Core/UserInterface/CommandCreatedEventArgs.h>
#include <Core/UserInterface/CommandEvent.h>
#include <Core/UserInterface/CommandEventArgs.h>
#include <Core/UserInterface/CommandEventHandler.h>
#include <Core/UserInterface/ValidateInputsEventHandler.h>
#include <Core/UserInterface/ValidateInputsEvent.h>
#include <Core/UserInterface/ValidateInputsEventArgs.h>
#include <Core/UserInterface/InputChangedEventHandler.h>
#include <Core/UserInterface/InputChangedEvent.h>
#include <Core/UserInterface/InputChangedEventArgs.h>
#include <Core/UserInterface/Command.h>
#include <Core/UserInterface/CommandDefinition.h>
#include <Core/UserInterface/CommandDefinitions.h>
#include <Core/UserInterface/CommandInputs.h>
#include <Core/UserInterface/ValueCommandInput.h>
#include <Core/UserInterface/StringValueCommandInput.h>
#include <Core/UserInterface/DropDownCommandInput.h>
#include <Core/UserInterface/ListItems.h>
#include <Core/UserInterface/ListItem.h>
#include <Core/UserInterface/ImageCommandInput.h>
#include <Core/UserInterface/TextBoxCommandInput.h>
#include <Core/UserInterface/ToolbarPanelList.h>
#include <Core/UserInterface/ToolbarPanels.h>
#include <Core/UserInterface/ToolbarPanel.h>
#include <Core/UserInterface/ToolbarControls.h>
#include <Core/UserInterface/ToolbarControl.h>
#include <Core/UserInterface/CommandControl.h>
#include <Core/UserInterface/Workspaces.h>
#include <Core/UserInterface/Workspace.h>
#include <Core/Application/Attributes.h>
#include <Core/Application/Attribute.h>
#include <Fusion/Fusion/Design.h>
#include <Fusion/Components/Component.h>
#include <Fusion/Components/Occurrences.h>
#include <Fusion/Components/Occurrence.h>
#include <Fusion/BRep/BRepFaces.h>
#include <Fusion/BRep/BRepFace.h>
#include <Fusion/BRep/BRepEdges.h>
#include <Fusion/BRep/BRepEdge.h>
#include <Fusion/BRep/BRepBody.h>
#include <Fusion/Construction/ConstructionPlane.h>
#include <Fusion/Sketch/Sketches.h>
#include <Fusion/Sketch/Sketch.h>
#include <Fusion/Sketch/SketchCurves.h>
#include <Fusion/Sketch/SketchLines.h>
#include <Fusion/Sketch/SketchLine.h>
#include <Fusion/Sketch/SketchArcs.h>
#include <Fusion/Sketch/SketchArc.h>
#include <Fusion/Sketch/SketchPoint.h>
#include <Fusion/Sketch/SketchFittedSplines.h>
#include <Fusion/Sketch/SketchFittedSpline.h>
#include <Fusion/Sketch/SketchCircles.h>
#include <Fusion/Sketch/SketchCircle.h>
#include <Fusion/Sketch/GeometricConstraints.h>
#include <Fusion/Sketch/TangentConstraint.h>
#include <Fusion/Sketch/Profile.h>
#include <Fusion/Sketch/Profiles.h>
#include <Fusion/Sketch/ProfileLoops.h>
#include <Fusion/Features/Features.h>
#include <Fusion/Features/ExtrudeFeatures.h>
#include <Fusion/Features/ExtrudeFeatureInput.h>
#include <Fusion/Features/ExtrudeFeature.h>
#include <Fusion/Features/FilletFeatures.h>
#include <Fusion/Features/FilletFeatureInput.h>
#include <Fusion/Features/FilletFeature.h>
#include <Fusion/Features/CircularPatternFeatures.h>
#include <Fusion/Features/CircularPatternFeatureInput.h>
#include <Fusion/Features/CircularPatternFeature.h>
#include <Fusion/Fusion/Timeline.h>
#include <Fusion/Fusion/TimelineObject.h>
#include <Fusion/Fusion/TimelineGroups.h>
#include <Fusion/Fusion/TimelineGroup.h>

#include <sstream>
#define _USE_MATH_DEFINES
#include <math.h>
#include <memory>

#if defined(_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace adsk::core;
using namespace adsk::fusion;

// Globals
Ptr<Application> _app;
Ptr<UserInterface> _ui;
std::string _units = "";

// Global command input declarations.
Ptr<ImageCommandInput> _imgInputEnglish;
Ptr<ImageCommandInput> _imgInputMetric;
Ptr<DropDownCommandInput> _standard;
Ptr<DropDownCommandInput> _pressureAngle;
Ptr<ValueCommandInput> _pressureAngleCustom;
Ptr<ValueCommandInput> _backlash;
Ptr<ValueCommandInput> _diaPitch;
Ptr<ValueCommandInput> _module;
Ptr<StringValueCommandInput> _numTeeth;
Ptr<ValueCommandInput> _rootFilletRad;
Ptr<ValueCommandInput> _thickness;
Ptr<ValueCommandInput> _holeDiam;
Ptr<TextBoxCommandInput> _pitchDiam;
Ptr<TextBoxCommandInput> _errMessage;

bool getCommandInputValue(Ptr<CommandInput> commandInput, std::string unitType, double* value);
bool is_digits(const std::string& str);
Ptr<Component> drawGear(
    Ptr<Design> design,
    double diametralPitch,
    int numTeeth,
    double thickness,
    double rootFilletRad,
    double pressureAngle,
    double backlash,
    double holeDiam);

bool checkReturn(Ptr<Base> returnObj)
{
    if (returnObj)
        return true;
    else if (_app && _ui)
    {
        std::string errDesc;
        _app->getLastError(&errDesc);
        _ui->messageBox(errDesc);
        return false;
    }
    else
        return false;
}

// Event handler for the execute event.
class GearCommandExecuteEventHandler : public adsk::core::CommandEventHandler
{
  public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override
    {
        double diaPitch = 0.0;
        if (_standard->selectedItem()->name() == "English")
        {
            diaPitch = _diaPitch->value();
        }
        else if (_standard->selectedItem()->name() == "Metric")
        {
            diaPitch = 25.4 / _module->value();
        }

        // Save the current values as attributes.
        Ptr<Design> des = _app->activeProduct();
        Ptr<Attributes> attribs = des->attributes();
        attribs->add("SpurGear", "standard", _standard->selectedItem()->name());
        attribs->add("SpurGear", "pressureAngle", _pressureAngle->selectedItem()->name());
        attribs->add("SpurGear", "pressureAngleCustom", std::to_string(_pressureAngleCustom->value()));
        attribs->add("SpurGear", "diaPitch", std::to_string(diaPitch));
        attribs->add("SpurGear", "numTeeth", _numTeeth->value());
        attribs->add("SpurGear", "rootFilletRad", std::to_string(_rootFilletRad->value()));
        attribs->add("SpurGear", "thickness", std::to_string(_thickness->value()));
        attribs->add("SpurGear", "holeDiam", std::to_string(_holeDiam->value()));
        attribs->add("SpurGear", "backlash", std::to_string(_backlash->value()));

        // Get the current values.
        double pressureAngle = 0.0;
        if (_pressureAngle->selectedItem()->name() == "Custom")
        {
            pressureAngle = _pressureAngleCustom->value();
        }
        else
        {
            if (_pressureAngle->selectedItem()->name() == "14.5 deg")
            {
                pressureAngle = 14.5 * (M_PI / 180.0);
            }
            else if (_pressureAngle->selectedItem()->name() == "20 deg")
            {
                pressureAngle = 20.0 * (M_PI / 180.0);
            }
            else if (_pressureAngle->selectedItem()->name() == "25 deg")
            {
                pressureAngle = 25.0 * (M_PI / 180);
            }
        }

        int numTeeth = std::stoi(_numTeeth->value());
        double rootFilletRad = _rootFilletRad->value();
        double thickness = _thickness->value();
        double holeDiam = _holeDiam->value();
        double backlash = _backlash->value();

        // Create the gear.
        Ptr<Component> gearComp;
        gearComp = drawGear(des, diaPitch, numTeeth, thickness, rootFilletRad, pressureAngle, backlash, holeDiam);

        if (gearComp)
        {
            std::string desc = "";
            if (_standard->selectedItem()->name() == "English")
            {
                desc = "Spur Gear; Diametrial Pitch: " + std::to_string(diaPitch) + "; ";
            }
            else if (_standard->selectedItem()->name() == "Metric")
            {
                desc = "Spur Gear; Module: " + std::to_string(25.4 / diaPitch) + "; ";
            }

            desc += "Num Teeth: " + std::to_string(numTeeth) + "; ";
            desc += "Pressure Angle: " + std::to_string(pressureAngle * (180 / M_PI)) + "; ";

            desc += "Backlash: " + des->unitsManager()->formatInternalValue(backlash, _units, true);
            gearComp->description(desc);
        }
        else
        {
            eventArgs->executeFailed(true);
            eventArgs->executeFailedMessage("Unexpected failure while constructing the gear.");
        }
    }
} _gearCommandExecute;

class GearCommandInputChangedHandler : public adsk::core::InputChangedEventHandler
{
  public:
    void notify(const Ptr<InputChangedEventArgs>& eventArgs) override
    {
        Ptr<CommandInput> changedInput = eventArgs->input();

        if (changedInput->id() == "standard")
        {
            if (_standard->selectedItem()->name() == "English")
            {
                _imgInputMetric->isVisible(false);
                _imgInputEnglish->isVisible(true);

                _diaPitch->isVisible(true);
                _module->isVisible(false);

                _diaPitch->value(25.4 / _module->value());

                _units = "in";
            }
            else if (_standard->selectedItem()->name() == "Metric")
            {
                _imgInputMetric->isVisible(true);
                _imgInputEnglish->isVisible(false);

                _diaPitch->isVisible(false);
                _module->isVisible(true);

                _module->value(25.4 / _diaPitch->value());

                _units = "mm";
            }

            // Set each one to it's current value because otherwised if the user
            // has edited it, the value won't update in the dialog because
            // apparently it remembers the units when the value was edited.
            // Setting the value using the API resets this.
            _backlash->value(_backlash->value());
            _backlash->unitType(_units);
            _rootFilletRad->value(_rootFilletRad->value());
            _rootFilletRad->unitType(_units);
            _thickness->value(_thickness->value());
            _thickness->unitType(_units);
            _holeDiam->value(_holeDiam->value());
            _holeDiam->unitType(_units);
        }

        // Update the pitch diameter value.
        double diaPitch = 0;
        if (_standard->selectedItem()->name() == "English")
        {
            double value;
            if (getCommandInputValue(_diaPitch, "", &value))
            {
                diaPitch = value;
            }
        }
        else if (_standard->selectedItem()->name() == "Metric")
        {
            double value;
            if (getCommandInputValue(_module, "", &value))
            {
                diaPitch = 25.4 / value;
            }
        }

        if (diaPitch != 0)
        {
            if (is_digits(_numTeeth->value()))
            {
                double numTeeth = std::stoi(_numTeeth->value());
                double pitchDia = numTeeth / diaPitch;

                // The pitch dia has been calculated in inches, but this expects cm as the input units.
                Ptr<Design> des = _app->activeProduct();
                std::string pitchDiaText = des->unitsManager()->formatInternalValue(pitchDia * 2.54, _units, true);
                _pitchDiam->text(pitchDiaText);
            }
            else
            {
                _pitchDiam->text("");
            }
        }
        else
        {
            _pitchDiam->text("");
        }

        if (changedInput->id() == "pressureAngle")
        {
            if (_pressureAngle->selectedItem()->name() == "Custom")
            {
                _pressureAngleCustom->isVisible(true);
            }
            else
            {
                _pressureAngleCustom->isVisible(false);
            }
        }
    }
} _gearCommandInputChanged;

class GearCommandValidateInputsEventHandler : public adsk::core::ValidateInputsEventHandler
{
  public:
    void notify(const Ptr<ValidateInputsEventArgs>& eventArgs) override
    {
        _errMessage->text("");

        // Verify that at lesat 4 teath are specified.
        int numTeeth;
        if (!is_digits(_numTeeth->value()))
        {
            _errMessage->text("The number of teeth must be a whole number.");
            eventArgs->areInputsValid(false);
            return;
        }
        else
        {
            numTeeth = std::stoi(_numTeeth->value());
        }

        if (numTeeth < 4)
        {
            _errMessage->text("The number of teeth must be 4 or more.");
            eventArgs->areInputsValid(false);
            return;
        }

        // Calculate some of the gear sizes to use in validation.
        double diaPitch = 0.0;
        if (_standard->selectedItem()->name() == "English")
        {
            double value;
            if (!getCommandInputValue(_diaPitch, "", &value))
            {
                eventArgs->areInputsValid(false);
                return;
            }
            else
            {
                diaPitch = value;
            }
        }
        else if (_standard->selectedItem()->name() == "Metric")
        {
            double value;
            if (!getCommandInputValue(_module, "", &value))
            {
                eventArgs->areInputsValid(false);
                return;
            }
            else
            {
                diaPitch = 25.4 / value;
            }
        }

        double diametralPitch = diaPitch / 2.54;
        double pitchDia = numTeeth / diametralPitch;

        double dedendum;
        if (diametralPitch < (20.0 * (M_PI / 180.0)) - 0.000001)
        {
            dedendum = 1.157 / diametralPitch;
        }
        else
        {
            double circularPitch = M_PI / diametralPitch;
            if (circularPitch >= 20.0)
            {
                dedendum = 1.25 / diametralPitch;
            }
            else
            {
                dedendum = (1.2 / diametralPitch) + (.002 * 2.54);
            }
        }

        double rootDia = pitchDia - (2.0 * dedendum);

        double pressureAngle = 0.0;
        if (_pressureAngle->selectedItem()->name() == "Custom")
        {
            pressureAngle = _pressureAngleCustom->value();
        }
        else
        {
            if (_pressureAngle->selectedItem()->name() == "14.5 deg")
                pressureAngle = 14.5 * (M_PI / 180.0);
            else if (_pressureAngle->selectedItem()->name() == "20 deg")
                pressureAngle = 20.0 * (M_PI / 180.0);
            else if (_pressureAngle->selectedItem()->name() == "25 deg")
                pressureAngle = 25.0 * (M_PI / 180.0);
        }

        double baseCircleDia = pitchDia * cos(pressureAngle);
        double baseCircleCircumference = 2.0 * M_PI * (baseCircleDia / 2.0);

        Ptr<Design> des = _app->activeProduct();

        double holeDiam;
        double value;
        if (!getCommandInputValue(_holeDiam, _units, &value))
        {
            eventArgs->areInputsValid(false);
            return;
        }
        else
        {
            holeDiam = value;
        }

        if (holeDiam >= (rootDia - 0.01))
        {
            _errMessage->text(
                "The center hole diameter is too large.  It must be less than " +
                des->unitsManager()->formatInternalValue(rootDia - 0.01, _units, true));
            eventArgs->areInputsValid(false);
            return;
        }

        double toothThickness = baseCircleCircumference / (numTeeth * 2);
        if (_rootFilletRad->value() > toothThickness * 0.4)
        {
            _errMessage->text(
                "The root fillet radius is too large.  It must be less than " +
                des->unitsManager()->formatInternalValue(toothThickness * 0.4, _units, true));
            eventArgs->areInputsValid(false);
            return;
        }
    }
} _gearCommandValidateInputs;

class SpurGearCommandDestroyEventHandler : public adsk::core::CommandEventHandler
{
  public:
    void notify(const Ptr<CommandEventArgs>& eventArgs) override
    {
        // Terminate the script since the command has finished.
        adsk::terminate();
    }
} _gearCommandDestroy;

class SpurGearCommandCreatedEventHandler : public adsk::core::CommandCreatedEventHandler
{
  public:
    void notify(const Ptr<CommandCreatedEventArgs>& eventArgs) override
    {
        // Verify that a Fusion design is active.
        Ptr<Design> des = _app->activeProduct();
        if (!checkReturn(des))
        {
            _ui->messageBox("The DESIGN workspace must be active when running this script.");
            return;
        }

        std::string defaultUnits = des->unitsManager()->defaultLengthUnits();

        // Determine whether to use inches or millimeters as the intial default.
        if (defaultUnits == "in" || defaultUnits == "ft")
        {
            _units = "in";
        }
        else
        {
            _units = "mm";
        }

        // Define the default values and get the previous values from the attributes.
        std::string standard;
        if (_units == "in")
        {
            standard = "English";
        }
        else
        {
            standard = "Metric";
        }

        Ptr<Attribute> standardAttrib = des->attributes()->itemByName("SpurGear", "standard");
        if (checkReturn(standardAttrib))
            standard = standardAttrib->value();

        if (standard == "English")
        {
            _units = "in";
        }
        else
        {
            _units = "mm";
        }

        std::string pressureAngle = "20 deg";
        Ptr<Attribute> pressureAngleAttrib = des->attributes()->itemByName("SpurGear", "pressureAngle");
        if (checkReturn(pressureAngleAttrib))
        {
            pressureAngle = pressureAngleAttrib->value();
        }

        double pressureAngleCustom = 20 * (M_PI / 180.0);
        Ptr<Attribute> pressureAngleCustomAttrib = des->attributes()->itemByName("SpurGear", "pressureAngleCustom");
        if (checkReturn(pressureAngleCustomAttrib))
        {
            pressureAngleCustom = std::stod(pressureAngleCustomAttrib->value());
        }

        std::string diaPitch = "2";
        Ptr<Attribute> diaPitchAttrib = des->attributes()->itemByName("SpurGear", "diaPitch");
        if (checkReturn(diaPitchAttrib))
        {
            diaPitch = diaPitchAttrib->value();
        }
        double metricModule = 25.4 / std::stod(diaPitch);

        std::string backlash = "0";
        Ptr<Attribute> backlashAttrib = des->attributes()->itemByName("SpurGear", "backlash");
        if (checkReturn(backlashAttrib))
            backlash = backlashAttrib->value();

        std::string numTeeth = "24";
        Ptr<Attribute> numTeethAttrib = des->attributes()->itemByName("SpurGear", "numTeeth");
        if (checkReturn(numTeethAttrib))
            numTeeth = numTeethAttrib->value();

        std::string rootFilletRad = std::to_string(.0625 * 2.54);
        Ptr<Attribute> rootFilletRadAttrib = des->attributes()->itemByName("SpurGear", "rootFilletRad");
        if (checkReturn(rootFilletRadAttrib))
            rootFilletRad = rootFilletRadAttrib->value();

        std::string thickness = std::to_string(0.5 * 2.54);
        Ptr<Attribute> thicknessAttrib = des->attributes()->itemByName("SpurGear", "thickness");
        if (checkReturn(thicknessAttrib))
            thickness = thicknessAttrib->value();

        std::string holeDiam = std::to_string(0.5 * 2.54);
        Ptr<Attribute> holeDiamAttrib = des->attributes()->itemByName("SpurGear", "holeDiam");
        if (checkReturn(holeDiamAttrib))
            holeDiam = holeDiamAttrib->value();

        Ptr<Command> cmd = eventArgs->command();
        cmd->isExecutedWhenPreEmpted(false);
        Ptr<CommandInputs> inputs = cmd->commandInputs();
        if (!checkReturn(inputs))
            return;

        // Define the command dialog.
        _imgInputEnglish = inputs->addImageCommandInput("gearImageEnglish", "", "Resources/GearEnglish.png");
        if (!checkReturn(_imgInputEnglish))
            return;
        _imgInputEnglish->isFullWidth(true);

        _imgInputMetric = inputs->addImageCommandInput("gearImageMetric", "", "Resources/GearMetric.png");
        if (!checkReturn(_imgInputMetric))
            return;
        _imgInputMetric->isFullWidth(true);

        _standard = inputs->addDropDownCommandInput("standard", "Standard", TextListDropDownStyle);
        if (!checkReturn(_standard))
            return;

        if (standard == "English")
        {
            _standard->listItems()->add("English", true);
            _standard->listItems()->add("Metric", false);
            _imgInputMetric->isVisible(false);
        }
        else
        {
            _standard->listItems()->add("English", false);
            _standard->listItems()->add("Metric", true);
            _imgInputEnglish->isVisible(false);
        }

        _pressureAngle = inputs->addDropDownCommandInput("pressureAngle", "Pressure Angle", TextListDropDownStyle);
        if (!checkReturn(_pressureAngle))
            return;

        if (pressureAngle == "14.5 deg")
        {
            _pressureAngle->listItems()->add("14.5 deg", true);
        }
        else
        {
            _pressureAngle->listItems()->add("14.5 deg", false);
        }

        if (pressureAngle == "20 deg")
        {
            _pressureAngle->listItems()->add("20 deg", true);
        }
        else
        {
            _pressureAngle->listItems()->add("20 deg", false);
        }

        if (pressureAngle == "25 deg")
        {
            _pressureAngle->listItems()->add("25 deg", true);
        }
        else
        {
            _pressureAngle->listItems()->add("25 deg", false);
        }

        if (pressureAngle == "Custom")
        {
            _pressureAngle->listItems()->add("Custom", true);
        }
        else
        {
            _pressureAngle->listItems()->add("Custom", false);
        }

        _pressureAngleCustom = inputs->addValueInput(
            "pressureAngleCustom", "Custom Angle", "deg", ValueInput::createByReal(pressureAngleCustom));
        if (!checkReturn(_pressureAngleCustom))
            return;
        if (pressureAngle != "Custom")
        {
            _pressureAngleCustom->isVisible(false);
        }

        _diaPitch = inputs->addValueInput("diaPitch", "Diametral Pitch", "", ValueInput::createByString(diaPitch));

        _module = inputs->addValueInput("module", "Module", "", ValueInput::createByReal(metricModule));
        if (!checkReturn(_module))
            return;

        if (standard == "English")
        {
            _module->isVisible(false);
        }
        else if (standard == "Metric")
        {
            _diaPitch->isVisible(false);
        }

        _numTeeth = inputs->addStringValueInput("numTeeth", "Number of Teeth", numTeeth);
        if (!checkReturn(_pressureAngleCustom))
            return;

        _backlash =
            inputs->addValueInput("backlash", "Backlash", _units, ValueInput::createByReal(std::stod(backlash)));
        if (!checkReturn(_backlash))
            return;

        _rootFilletRad = inputs->addValueInput(
            "rootFilletRad", "Root Fillet Radius", _units, ValueInput::createByReal(std::stod(rootFilletRad)));
        if (!checkReturn(_rootFilletRad))
            return;

        _thickness = inputs->addValueInput(
            "thickness", "Gear Thickness", _units, ValueInput::createByReal(std::stod(thickness)));
        if (!checkReturn(_thickness))
            return;

        _holeDiam =
            inputs->addValueInput("holeDiam", "Hole Diameter", _units, ValueInput::createByReal(std::stod(holeDiam)));
        if (!checkReturn(_holeDiam))
            return;

        _pitchDiam = inputs->addTextBoxCommandInput("pitchDiam", "Pitch Diameter", "", 1, true);
        if (!checkReturn(_pitchDiam))
            return;

        _errMessage = inputs->addTextBoxCommandInput("errMessage", "", "", 2, true);
        if (!checkReturn(_errMessage))
            return;
        _errMessage->isFullWidth(true);

        // Connect to the command related events.
        Ptr<InputChangedEvent> inputChangedEvent = cmd->inputChanged();
        if (!inputChangedEvent)
            return;
        bool isOk = inputChangedEvent->add(&_gearCommandInputChanged);
        if (!isOk)
            return;

        Ptr<ValidateInputsEvent> validateInputsEvent = cmd->validateInputs();
        if (!validateInputsEvent)
            return;
        isOk = validateInputsEvent->add(&_gearCommandValidateInputs);
        if (!isOk)
            return;

        Ptr<CommandEvent> executeEvent = cmd->execute();
        if (!executeEvent)
            return;
        isOk = executeEvent->add(&_gearCommandExecute);
        if (!isOk)
            return;

        Ptr<CommandEvent> destroyEvent = cmd->destroy();
        if (!destroyEvent)
            return;

        isOk = destroyEvent->add(&_gearCommandDestroy);
        if (!isOk)
            return;
    }
} _gearCommandCreated;

extern "C" XI_EXPORT bool run(const char* context)
{
    _app = Application::get();
    if (!_app)
        return false;

    _ui = _app->userInterface();
    if (!_ui)
        return false;

    // Create a command definition and add a button to the CREATE panel.
    Ptr<CommandDefinition> cmdDef = _ui->commandDefinitions()->itemById("adskSpurGearCPPScript");
    if (!cmdDef)
    {
        cmdDef = _ui->commandDefinitions()->addButtonDefinition(
            "adskSpurGearCPPScript", "Spur Gear", "Creates a spur gear component", "Resources/SpurGear");
        if (!checkReturn(cmdDef))
            return false;
    }

    // Connect to the command created event.
    Ptr<CommandCreatedEvent> commandCreatedEvent = cmdDef->commandCreated();
    if (!checkReturn(commandCreatedEvent))
        return false;
    bool isOk = commandCreatedEvent->add(&_gearCommandCreated);
    if (!isOk)
        return false;

    isOk = cmdDef->execute();
    if (!isOk)
        return false;

    // Prevent this module from terminating so that the command can continue to run until
    // the user completes the command.
    adsk::autoTerminate(false);

    return true;
}

bool is_digits(const std::string& str)
{
    return str.find_first_not_of("0123456789") == std::string::npos;
}

/*
 *  Verfies that a value command input has a valid expression and returns the
 *  value if it does.  Otherwise it returns False.  This works around a
 *  problem where when you get the value from a ValueCommandInput it causes the
 *  current expression to be evaluated and updates the display.  Some new functionality
 *  is being added in the future to the ValueCommandInput object that will make
 *  this easier and should make this function obsolete.
 */
bool getCommandInputValue(Ptr<CommandInput> commandInput, std::string unitType, double* value)
{
    Ptr<ValueCommandInput> valCommandInput = commandInput;
    if (!commandInput)
    {
        *value = 0;
        return false;
    }

    // Verify that the expression is valid.
    Ptr<Design> des = _app->activeProduct();
    Ptr<UnitsManager> unitsMgr = des->unitsManager();

    if (unitsMgr->isValidExpression(valCommandInput->expression(), unitType))
    {
        *value = unitsMgr->evaluateExpression(valCommandInput->expression(), unitType);
        return true;
    }
    else
    {
        *value = 0;
        return false;
    }
}

// Calculate points along an involute curve.
Ptr<Point3D> involutePoint(double baseCircleRadius, double distFromCenterToInvolutePoint)
{
    // Calculate the other side of the right-angle triangle defined by the base circle and the current distance radius.
    // This is also the length of the involute chord as it comes off of the base circle.
    double triangleSide = sqrt(pow(distFromCenterToInvolutePoint, 2.0) - pow(baseCircleRadius, 2.0));

    // Calculate the angle of the involute.
    double alpha = triangleSide / baseCircleRadius;

    // Calculate the angle where the current involute point is.
    double theta = alpha - acos(baseCircleRadius / distFromCenterToInvolutePoint);

    // Calculate the coordinates of the involute point.
    double x = distFromCenterToInvolutePoint * cos(theta);
    double y = distFromCenterToInvolutePoint * sin(theta);

    // Create a point to return.
    return (adsk::core::Point3D::create(x, y, 0));
}

// Builds a spur gear.
Ptr<Component> drawGear(
    Ptr<Design> design,
    double diametralPitch,
    int numTeeth,
    double thickness,
    double rootFilletRad,
    double pressureAngle,
    double backlash,
    double holeDiam)
{
    // The diametral pitch is specified in inches but everthing
    // here expects all distances to be in centimeters, so convert
    // for the gear creation.
    diametralPitch = diametralPitch / 2.54;

    // Compute the various values for a gear.
    double pitchDia = (double)numTeeth / diametralPitch;

    // addendum = 1.0 / diametralPitch
    double dedendum;
    if (diametralPitch < (20 * (M_PI / 180.0)) - 0.000001)
    {
        dedendum = 1.157 / diametralPitch;
    }
    else
    {
        double circularPitch = M_PI / diametralPitch;
        if (circularPitch >= 20.0)
        {
            dedendum = 1.25 / diametralPitch;
        }
        else
        {
            dedendum = (1.2 / diametralPitch) + (.002 * 2.54);
        }
    }

    double rootDia = pitchDia - (2.0 * dedendum);

    double baseCircleDia = pitchDia * cos(pressureAngle);
    double outsideDia = (double)(numTeeth + 2) / diametralPitch;

    // Create a new component by creating an occurrence.
    Ptr<Occurrences> occs = design->rootComponent()->occurrences();
    if (!checkReturn(occs))
        return nullptr;

    Ptr<Matrix3D> mat = adsk::core::Matrix3D::create();
    if (!checkReturn(mat))
        return nullptr;

    Ptr<Occurrence> newOcc = occs->addNewComponent(mat);
    if (!checkReturn(newOcc))
        return nullptr;

    Ptr<Component> newComp = newOcc->component();
    if (!checkReturn(newComp))
        return nullptr;

    // Create a new sketch.
    Ptr<Sketches> sketches = newComp->sketches();
    if (!checkReturn(sketches))
        return nullptr;

    Ptr<ConstructionPlane> xyPlane = newComp->xYConstructionPlane();
    if (!checkReturn(xyPlane))
        return nullptr;

    Ptr<Sketch> baseSketch = sketches->add(xyPlane);
    if (!checkReturn(xyPlane))
        return nullptr;

    // Draw a circle for the base.
    baseSketch->sketchCurves()->sketchCircles()->addByCenterRadius(adsk::core::Point3D::create(0, 0, 0), rootDia / 2.0);
    if (!checkReturn(baseSketch))
        return nullptr;

    // Draw a circle for the center hole, if the value is greater than 0.
    Ptr<Profile> prof = nullptr;
    if (holeDiam - (_app->pointTolerance() * 2) > 0)
    {
        Ptr<SketchCircle> circ = baseSketch->sketchCurves()->sketchCircles()->addByCenterRadius(
            adsk::core::Point3D::create(0, 0, 0), holeDiam / 2.0);
        if (!checkReturn(circ))
            return nullptr;

        // Find the profile that uses both circles.
        for (Ptr<Profile> tempProf : baseSketch->profiles())
        {
            if (tempProf->profileLoops()->count() == 2)
            {
                prof = tempProf;
                break;
            }
        }
    }
    else
    {
        // Use the single profile.
        prof = baseSketch->profiles()->item(0);
    }

    if (!checkReturn(prof))
        return nullptr;

    //////// Extrude the circle to create the base of the gear.

    // Create an extrusion input to be able to define the input needed for an extrusion
    // while specifying the profile and that a new component is to be created
    Ptr<ExtrudeFeatures> extrudes = newComp->features()->extrudeFeatures();
    if (!checkReturn(extrudes))
        return nullptr;

    Ptr<ExtrudeFeatureInput> extInput = extrudes->createInput(prof, NewBodyFeatureOperation);
    if (!checkReturn(extInput))
        return nullptr;

    // Define that the extent is a distance extent of 5 cm.
    Ptr<ValueInput> distance = adsk::core::ValueInput::createByReal(thickness);
    if (!checkReturn(distance))
        return nullptr;

    bool result = extInput->setDistanceExtent(false, distance);
    if (!result)
        return nullptr;

    // Create the extrusion.
    Ptr<ExtrudeFeature> baseExtrude = extrudes->add(extInput);
    if (!checkReturn(baseExtrude))
        return nullptr;

    // Create a second sketch for the tooth.
    Ptr<Sketch> toothSketch = sketches->add(xyPlane);
    if (!checkReturn(toothSketch))
        return nullptr;

    // Calculate points along the involute curve.
    int involutePointCount = 15;
    double involuteIntersectionRadius = baseCircleDia / 2.0;
    Ptr<Point3D>* involutePoints = new Ptr<Point3D>[involutePointCount];
    std::unique_ptr<Ptr<Point3D>[]> involutePointsDeleter(involutePoints);
    double involuteSize = (outsideDia - baseCircleDia) / 2.0;
    for (int i = 0; i < involutePointCount; ++i)
    {
        involuteIntersectionRadius = (baseCircleDia / 2.0) + ((involuteSize / (involutePointCount - 1)) * i);
        Ptr<Point3D> newPoint = involutePoint(baseCircleDia / 2.0, involuteIntersectionRadius);
        involutePoints[i] = newPoint;
    }

    // Get the point along the tooth that's at the pictch diameter and then
    // calculate the angle to that point.
    Ptr<Point3D> pitchInvolutePoint = involutePoint(baseCircleDia / 2.0, pitchDia / 2.0);
    double pitchPointAngle = atan(pitchInvolutePoint->y() / pitchInvolutePoint->x());

    // Determine the angle defined by the tooth thickness as measured at
    // the pitch diameter circle.
    double toothThicknessAngle = (2.0 * M_PI) / (2.0 * (double)numTeeth);

    // Determine the angle needed for the specified backlash.
    double backlashAngle = (backlash / (pitchDia / 2.0)) * 0.25;

    // Determine the angle to rotate the curve.
    double rotateAngle = -((toothThicknessAngle / 2.0) + pitchPointAngle - backlashAngle);

    // Rotate the involute so the middle of the tooth lies on the x axis.
    double cosAngle = cos(rotateAngle);
    double sinAngle = sin(rotateAngle);
    for (int i = 0; i < involutePointCount; ++i)
    {
        double newX = involutePoints[i]->x() * cosAngle - involutePoints[i]->y() * sinAngle;
        double newY = involutePoints[i]->x() * sinAngle + involutePoints[i]->y() * cosAngle;
        involutePoints[i] = adsk::core::Point3D::create(newX, newY, 0);
    }

    // Create a new set of points with a negated y.  This effectively mirrors the original
    // points about the X axis.
    Ptr<Point3D>* involute2Points = new Ptr<Point3D>[involutePointCount];
    std::unique_ptr<Ptr<Point3D>[]> involute2PointsDeleter(involute2Points);
    for (int i = 0; i < involutePointCount; ++i)
    {
        involute2Points[i] = adsk::core::Point3D::create(involutePoints[i]->x(), -involutePoints[i]->y(), 0);
    }

    double* curve1Angle = new double[involutePointCount];
    std::unique_ptr<double[]> curve1AngleDeleter(curve1Angle);
    for (int i = 0; i < involutePointCount; ++i)
    {
        curve1Angle[i] = atan(involutePoints[i]->y() / involutePoints[i]->x());
    }

    double* curve2Angle = new double[involutePointCount];
    std::unique_ptr<double[]> curve2AngleDeleter(curve2Angle);
    for (int i = 0; i < involutePointCount; ++i)
    {
        curve2Angle[i] = atan(involute2Points[i]->y() / involute2Points[i]->x());
    }

    toothSketch->isComputeDeferred(true);

    // Create and load an object collection with the points.
    Ptr<ObjectCollection> pointSet = adsk::core::ObjectCollection::create();
    for (int i = 0; i < involutePointCount; ++i)
    {
        pointSet->add(involutePoints[i]);
    }

    // Create the first spline.
    Ptr<SketchFittedSpline> spline1 = toothSketch->sketchCurves()->sketchFittedSplines()->add(pointSet);
    if (!checkReturn(spline1))
        return nullptr;

    // Add the involute points for the second spline to an ObjectCollection.
    pointSet = adsk::core::ObjectCollection::create();
    for (int i = 0; i < involutePointCount; ++i)
    {
        pointSet->add(involute2Points[i]);
    }

    // Create the second spline.
    Ptr<SketchFittedSpline> spline2 = toothSketch->sketchCurves()->sketchFittedSplines()->add(pointSet);
    if (!checkReturn(spline2))
        return nullptr;

    // Draw the arc for the top of the tooth.
    Ptr<Point3D> midPoint = adsk::core::Point3D::create((outsideDia / 2.0), 0, 0);
    Ptr<SketchArc> topArc = toothSketch->sketchCurves()->sketchArcs()->addByThreePoints(
        spline1->endSketchPoint(), midPoint, spline2->endSketchPoint());
    if (!checkReturn(topArc))
        return nullptr;

    // Check to see if involute goes down to the root or not.  If not, then
    // create lines to connect the involute to the root.
    if (baseCircleDia < rootDia)
    {
        Ptr<SketchLine> bottomLine = toothSketch->sketchCurves()->sketchLines()->addByTwoPoints(
            spline2->startSketchPoint(), spline1->startSketchPoint());
        if (!checkReturn(bottomLine))
            return nullptr;
    }
    else
    {
        Ptr<Point3D> rootPoint1 = adsk::core::Point3D::create(
            (rootDia / 2 - 0.001) * cos(curve1Angle[0]), (rootDia / 2) * sin(curve1Angle[0]), 0);
        Ptr<SketchLine> line1 =
            toothSketch->sketchCurves()->sketchLines()->addByTwoPoints(rootPoint1, spline1->startSketchPoint());
        if (!checkReturn(line1))
            return nullptr;

        Ptr<Point3D> rootPoint2 = adsk::core::Point3D::create(
            (rootDia / 2 - 0.001) * cos(curve2Angle[0]), (rootDia / 2) * sin(curve2Angle[0]), 0);
        Ptr<SketchLine> line2 =
            toothSketch->sketchCurves()->sketchLines()->addByTwoPoints(rootPoint2, spline2->startSketchPoint());
        if (!checkReturn(line2))
            return nullptr;

        Ptr<SketchLine> bottomLine = toothSketch->sketchCurves()->sketchLines()->addByTwoPoints(
            line1->startSketchPoint(), line2->startSketchPoint());
        if (!checkReturn(bottomLine))
            return nullptr;

        // Make the lines tangent to the spline so the root fillet will behave correctly.
        if (!line1->isFixed(true))
            return nullptr;

        if (!line2->isFixed(true))
            return nullptr;

        Ptr<GeometricConstraints> geomConstraints = toothSketch->geometricConstraints();
        if (!checkReturn(geomConstraints))
            return nullptr;

        Ptr<TangentConstraint> tangent = geomConstraints->addTangent(spline1, line1);
        if (!checkReturn(tangent))
            return nullptr;

        tangent = geomConstraints->addTangent(spline2, line2);
        if (!checkReturn(tangent))
            return nullptr;
    }

    toothSketch->isComputeDeferred(false);

    ////// Extrude the tooth.

    // Get the profile defined by the tooth.
    prof = toothSketch->profiles()->item(0);
    if (!checkReturn(prof))
        return nullptr;

    // Create an extrusion input to be able to define the input needed for an extrusion
    // while specifying the profile and that a new component is to be created
    extInput = extrudes->createInput(prof, JoinFeatureOperation);
    if (!checkReturn(extInput))
        return nullptr;

    // Define that the extent is a distance extent of 5 cm.
    distance = adsk::core::ValueInput::createByReal(thickness);
    if (!checkReturn(distance))
        return nullptr;

    result = extInput->setDistanceExtent(false, distance);
    if (!result)
        return nullptr;

    // Create the extrusion.
    Ptr<ExtrudeFeature> toothExtrude = extrudes->add(extInput);
    if (!checkReturn(toothExtrude))
        return nullptr;

    Ptr<FilletFeature> baseFillet;
    if (rootFilletRad > 0)
    {
        ////// Find the edges between the base cylinder and the tooth.

        // Get the outer cylindrical face from the base extrusion by checking the number
        // of edges and if it's 2 get the other one.
        Ptr<BRepFace> cylFace = baseExtrude->sideFaces()->item(0);
        if (cylFace->edges()->count() == 2)
        {
            cylFace = baseExtrude->sideFaces()->item(1);
        }
        if (!checkReturn(cylFace))
            return nullptr;

        // Get the two linear edges, which are the connection between the cylinder and tooth.
        Ptr<ObjectCollection> edges = adsk::core::ObjectCollection::create();
        for (Ptr<BRepEdge> edge : cylFace->edges())
        {
            Ptr<Line3D> tempLine = edge->geometry();
            if (tempLine)
            {
                edges->add(edge);
            }
        }

        // Create a fillet input to be able to define the input needed for a fillet.
        Ptr<FilletFeatures> fillets = newComp->features()->filletFeatures();
        if (!checkReturn(fillets))
            return nullptr;

        Ptr<FilletFeatureInput> filletInput = fillets->createInput();
        if (!checkReturn(filletInput))
            return nullptr;

        // Define that the extent is a distance extent of 5 cm.
        Ptr<ValueInput> radius = adsk::core::ValueInput::createByReal(rootFilletRad);
        if (!checkReturn(radius))
            return nullptr;

        result = filletInput->addConstantRadiusEdgeSet(edges, radius, false);
        if (!result)
            return nullptr;

        // Create the extrusion.
        baseFillet = fillets->add(filletInput);
        if (!checkReturn(baseFillet))
            return nullptr;
    }

    // Create a pattern of the tooth extrude and the base fillet.
    Ptr<CircularPatternFeatures> circularPatterns = newComp->features()->circularPatternFeatures();
    if (!checkReturn(circularPatterns))
        return nullptr;

    Ptr<ObjectCollection> entities = adsk::core::ObjectCollection::create();
    entities->add(toothExtrude);
    if (baseFillet)
    {
        entities->add(baseFillet);
    }

    Ptr<BRepFace> cylFace = baseExtrude->sideFaces()->item(0);
    if (!checkReturn(cylFace))
        return nullptr;

    Ptr<CircularPatternFeatureInput> patternInput = circularPatterns->createInput(entities, cylFace);
    if (!checkReturn(patternInput))
        return nullptr;

    Ptr<ValueInput> numTeethInput = adsk::core::ValueInput::createByString(std::to_string(numTeeth));
    if (!checkReturn(numTeethInput))
        return nullptr;

    patternInput->quantity(numTeethInput);
    patternInput->patternComputeOption(adsk::fusion::PatternComputeOptions::IdenticalPatternCompute);
    Ptr<CircularPatternFeature> pattern = circularPatterns->add(patternInput);
    if (!checkReturn(pattern))
        return nullptr;

    // Create an extra sketch that contains a circle of the diametral pitch.
    Ptr<Sketch> diametralPitchSketch = sketches->add(xyPlane);
    if (!checkReturn(diametralPitchSketch))
        return nullptr;

    Ptr<SketchCircle> diametralPitchCircle = diametralPitchSketch->sketchCurves()->sketchCircles()->addByCenterRadius(
        adsk::core::Point3D::create(0, 0, 0), pitchDia / 2.0);
    if (!checkReturn(diametralPitchCircle))
        return nullptr;

    diametralPitchCircle->isConstruction(true);
    diametralPitchCircle->isFixed(true);

    // Group everything used to create the gear in the timeline.
    Ptr<TimelineGroups> timelineGroups = design->timeline()->timelineGroups();
    if (!checkReturn(timelineGroups))
        return nullptr;

    int newOccIndex = newOcc->timelineObject()->index();
    int pitchSketchIndex = diametralPitchSketch->timelineObject()->index();

    Ptr<TimelineGroup> timelineGroup = timelineGroups->add(newOccIndex, pitchSketchIndex);
    if (!checkReturn(timelineGroup))
        return nullptr;

    timelineGroup->name("Spur Gear");

    // Add an attribute to the component with all of the input values.  This might
    // be used in the future to be able to edit the gear.
    std::string gearValues = "{";
    gearValues += "'pressureAngle': '" + std::to_string(pressureAngle) + "',";
    gearValues += "'numTeeth': '" + std::to_string(numTeeth) + "',";
    gearValues += "'backlash': '" + std::to_string(backlash) + "',";
    gearValues += "'holeDiam': '" + std::to_string(holeDiam) + "',";
    gearValues += "'thickness': '" + std::to_string(thickness) + "',";
    gearValues += "'rootFilletRad': '" + std::to_string(rootFilletRad) + "',";
    gearValues += "'diametralPitch': '" + std::to_string(diametralPitch * 2.54) + "'}";
    Ptr<Attribute> attrib = newComp->attributes()->add("SpurGear", "Values", gearValues);
    if (!checkReturn(attrib))
        return nullptr;

    // Set the name of the component.
    newComp->name("Spur Gear (" + std::to_string(numTeeth) + " teeth)");

    return newComp;
}
