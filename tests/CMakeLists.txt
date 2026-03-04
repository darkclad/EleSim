find_package(Qt6 REQUIRED COMPONENTS Test)

# Core + simulation sources needed by tests (no UI)
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/Component.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Node.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Wire.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Circuit.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Resistor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Capacitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/DCSource.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/ACSource.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/PulseSource.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/DCCurrentSource.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Lamp.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Junction.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Switch.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Switch3Way.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Switch4Way.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Inductor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Diode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/ZenerDiode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/NMosfet.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/PMosfet.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/NOTGate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/ANDGate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/ORGate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/XORGate.cpp
    ${CMAKE_SOURCE_DIR}/src/core/components/Ground.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/MNASolver.cpp
)

# --- AC Analysis Test ---
qt_add_executable(test_ac_analysis test_ac_analysis.cpp ${CORE_SOURCES})
target_link_libraries(test_ac_analysis PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_ac_analysis PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Pass test data directory as a define
target_compile_definitions(test_ac_analysis PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)

add_test(NAME ACAnalysis COMMAND test_ac_analysis)

# --- RL High-Pass Test ---
qt_add_executable(test_rl_highpass test_rl_highpass.cpp ${CORE_SOURCES})
target_link_libraries(test_rl_highpass PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_rl_highpass PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_rl_highpass PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME RLHighPass COMMAND test_rl_highpass)

# --- Series RLC Resonant Test ---
qt_add_executable(test_rlc_series test_rlc_series.cpp ${CORE_SOURCES})
target_link_libraries(test_rlc_series PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_rlc_series PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_rlc_series PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME RLCSeries COMMAND test_rlc_series)

# --- Half-Wave Rectifier + Filter Test ---
qt_add_executable(test_halfwave_rectifier test_halfwave_rectifier.cpp ${CORE_SOURCES})
target_link_libraries(test_halfwave_rectifier PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_halfwave_rectifier PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_halfwave_rectifier PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME HalfwaveRectifier COMMAND test_halfwave_rectifier)

# --- Voltage Divider (AC+DC) Mixed Test ---
qt_add_executable(test_voltage_divider_mixed test_voltage_divider_mixed.cpp ${CORE_SOURCES})
target_link_libraries(test_voltage_divider_mixed PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_voltage_divider_mixed PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_voltage_divider_mixed PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME VoltageDividerMixed COMMAND test_voltage_divider_mixed)

# --- Full-Wave Bridge Rectifier Test ---
qt_add_executable(test_bridge_rectifier test_bridge_rectifier.cpp ${CORE_SOURCES})
target_link_libraries(test_bridge_rectifier PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_bridge_rectifier PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_bridge_rectifier PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME BridgeRectifier COMMAND test_bridge_rectifier)

# --- RC Differentiator (Square Wave) Test ---
qt_add_executable(test_rc_differentiator test_rc_differentiator.cpp ${CORE_SOURCES})
target_link_libraries(test_rc_differentiator PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_rc_differentiator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_rc_differentiator PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME RCDifferentiator COMMAND test_rc_differentiator)

# --- Parallel LC Tank Test ---
qt_add_executable(test_parallel_lc_tank test_parallel_lc_tank.cpp ${CORE_SOURCES})
target_link_libraries(test_parallel_lc_tank PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_parallel_lc_tank PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_parallel_lc_tank PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME ParallelLCTank COMMAND test_parallel_lc_tank)

# --- Symmetrical Diode Clipper Test ---
qt_add_executable(test_diode_clipper test_diode_clipper.cpp ${CORE_SOURCES})
target_link_libraries(test_diode_clipper PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_diode_clipper PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_diode_clipper PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME DiodeClipper COMMAND test_diode_clipper)

# --- DC Current Source Test ---
qt_add_executable(test_current_source test_current_source.cpp ${CORE_SOURCES})
target_link_libraries(test_current_source PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_current_source PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_current_source PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME CurrentSource COMMAND test_current_source)

# --- Pulse Source Test ---
qt_add_executable(test_pulse_source test_pulse_source.cpp ${CORE_SOURCES})
target_link_libraries(test_pulse_source PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_pulse_source PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_pulse_source PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME PulseSource COMMAND test_pulse_source)

# --- Zener Diode Test ---
qt_add_executable(test_zener_diode test_zener_diode.cpp ${CORE_SOURCES})
target_link_libraries(test_zener_diode PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_zener_diode PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_zener_diode PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME ZenerDiode COMMAND test_zener_diode)

# --- N-Channel MOSFET Test ---
qt_add_executable(test_nmosfet test_nmosfet.cpp ${CORE_SOURCES})
target_link_libraries(test_nmosfet PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_nmosfet PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_nmosfet PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME NMosfet COMMAND test_nmosfet)

# --- P-Channel MOSFET Test ---
qt_add_executable(test_pmosfet test_pmosfet.cpp ${CORE_SOURCES})
target_link_libraries(test_pmosfet PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_pmosfet PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_pmosfet PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME PMosfet COMMAND test_pmosfet)

# --- NOT Gate Test ---
qt_add_executable(test_not_gate test_not_gate.cpp ${CORE_SOURCES})
target_link_libraries(test_not_gate PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_not_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_not_gate PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME NOTGate COMMAND test_not_gate)

# --- AND Gate Test ---
qt_add_executable(test_and_gate test_and_gate.cpp ${CORE_SOURCES})
target_link_libraries(test_and_gate PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_and_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_and_gate PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME ANDGate COMMAND test_and_gate)

# --- OR Gate Test ---
qt_add_executable(test_or_gate test_or_gate.cpp ${CORE_SOURCES})
target_link_libraries(test_or_gate PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_or_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_or_gate PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME ORGate COMMAND test_or_gate)

# --- XOR Gate Test ---
qt_add_executable(test_xor_gate test_xor_gate.cpp ${CORE_SOURCES})
target_link_libraries(test_xor_gate PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_xor_gate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_xor_gate PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME XORGate COMMAND test_xor_gate)

# --- AC Resistor + Lamp Loop Test ---
qt_add_executable(test_ac_resistor_lamp test_ac_resistor_lamp.cpp ${CORE_SOURCES})
target_link_libraries(test_ac_resistor_lamp PRIVATE Qt6::Core Qt6::Test Eigen3::Eigen)
target_include_directories(test_ac_resistor_lamp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_ac_resistor_lamp PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/circuits"
)
add_test(NAME ACResistorLamp COMMAND test_ac_resistor_lamp)
