# Browser contract test for the Qt kit's network layer on the web (qtclient/Qt5:
# QNetworkReply over fetch, QWebSocket over the browser WebSocket, JSPI event
# delivery). Built with the Qt wasm kit by build.sh; run by run.mjs.
cmake_minimum_required(VERSION 3.21)
project(qtnetwork_contract LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Network WebSockets)
qt_standard_project_setup()
qt_add_executable(qtnetwork_contract contract.cpp)
target_link_libraries(qtnetwork_contract PRIVATE Qt6::Core Qt6::Network Qt6::WebSockets)
