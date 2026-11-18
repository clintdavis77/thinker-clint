cmake_minimum_required(VERSION 3.31)
project(thinker_ability_tests CXX)
set(CMAKE_CXX_STANDARD 11)
enable_testing()

# Standalone Windows/MinGW tests: no game install or running game is required.
add_executable(workshop_abilities workshop_abilities.cpp ../src/ability.cpp)
target_compile_options(workshop_abilities PRIVATE -Wall -Wextra)
target_link_options(workshop_abilities PRIVATE -static)
add_test(NAME workshop_abilities COMMAND workshop_abilities)

add_executable(former_automation former_automation.cpp ../src/former_automation.cpp)
target_compile_options(former_automation PRIVATE -Wall -Wextra)
target_link_options(former_automation PRIVATE -static)
add_test(NAME former_automation COMMAND former_automation)

add_executable(former_movement former_movement.cpp ../src/former_automation.cpp ../src/former_plant_move.cpp)
target_compile_options(former_movement PRIVATE -Wall -Wextra)
target_link_options(former_movement PRIVATE -static)
add_test(NAME former_movement COMMAND former_movement)
