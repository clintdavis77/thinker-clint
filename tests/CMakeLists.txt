cmake_minimum_required(VERSION 3.31)
project(thinker_ability_tests CXX)
set(CMAKE_CXX_STANDARD 11)
enable_testing()

# Standalone Windows/MinGW tests: no game install or running game is required.
add_executable(workshop_abilities workshop_abilities.cpp ../src/ability.cpp)
target_compile_options(workshop_abilities PRIVATE -Wall -Wextra)
target_link_options(workshop_abilities PRIVATE -static)
add_test(NAME workshop_abilities COMMAND workshop_abilities)
