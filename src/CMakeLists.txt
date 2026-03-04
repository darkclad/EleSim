set(SOURCES
    main.cpp

    # Core data model
    core/Component.cpp
    core/Node.cpp
    core/Wire.cpp
    core/Circuit.cpp
    core/UndoManager.cpp
    core/components/Resistor.cpp
    core/components/Capacitor.cpp
    core/components/DCSource.cpp
    core/components/ACSource.cpp
    core/components/PulseSource.cpp
    core/components/DCCurrentSource.cpp
    core/components/Lamp.cpp
    core/components/Junction.cpp
    core/components/Switch.cpp
    core/components/Switch3Way.cpp
    core/components/Switch4Way.cpp
    core/components/Inductor.cpp
    core/components/Diode.cpp
    core/components/ZenerDiode.cpp
    core/components/NMosfet.cpp
    core/components/PMosfet.cpp
    core/components/NOTGate.cpp
    core/components/ANDGate.cpp
    core/components/ORGate.cpp
    core/components/XORGate.cpp
    core/components/Ground.cpp

    # Simulation
    simulation/MNASolver.cpp

    # UI
    ui/MainWindow.cpp
    ui/SchematicScene.cpp
    ui/SchematicView.cpp
    ui/ComponentToolbar.cpp
    ui/PropertyPanel.cpp
    ui/SimulationDialog.cpp
    ui/SplashScreen.cpp
    ui/OscilloscopeWidget.cpp

    # Graphics items
    ui/graphics/GraphicPin.cpp
    ui/graphics/GraphicComponent.cpp
    ui/graphics/GraphicWire.cpp
    ui/graphics/items/GResistor.cpp
    ui/graphics/items/GCapacitor.cpp
    ui/graphics/items/GDCSource.cpp
    ui/graphics/items/GACSource.cpp
    ui/graphics/items/GDCCurrentSource.cpp
    ui/graphics/items/GPulseSource.cpp
    ui/graphics/items/GLamp.cpp
    ui/graphics/items/GJunction.cpp
    ui/graphics/items/GSwitch.cpp
    ui/graphics/items/GSwitch3Way.cpp
    ui/graphics/items/GSwitch4Way.cpp
    ui/graphics/items/GInductor.cpp
    ui/graphics/items/GDiode.cpp
    ui/graphics/items/GZenerDiode.cpp
    ui/graphics/items/GNMosfet.cpp
    ui/graphics/items/GPMosfet.cpp
    ui/graphics/items/GNOTGate.cpp
    ui/graphics/items/GANDGate.cpp
    ui/graphics/items/GORGate.cpp
    ui/graphics/items/GXORGate.cpp
    ui/graphics/items/GGround.cpp
)

set(RESOURCES
    ${CMAKE_SOURCE_DIR}/resources/elesim.qrc
)

qt_add_executable(EleSim ${SOURCES} ${RESOURCES})
target_link_libraries(EleSim PRIVATE Qt6::Widgets Qt6::SvgWidgets Eigen3::Eigen)
target_include_directories(EleSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
