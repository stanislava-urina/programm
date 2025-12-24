#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <thread>
#include <map>
#include <string>
#include "../include/opcua_client.hpp"

using namespace ftxui;

int main() {
    OPCUAClient client;
    client.connectToServer("opc.tcp://127.0.0.1:4840");

    auto screen = ScreenInteractive::Fullscreen();
    std::map<std::string, std::vector<double>> histories;
    
    std::string input_val = "0.0";
    std::string status_text = "Status: Waiting...";
    int selected = 0;

    std::vector<std::string> names = {"Temperature", "Voltage"};
    
    auto menu = Menu(&names, &selected);
    auto input_field = Input(&input_val, "0.0");
    
    auto btn = Button(" SEND ", [&] {
        auto tags = client.getTags(); 
        if (selected < tags.size()) {
            try {
                double val = std::stod(input_val); 
                if (client.writeValue(tags[selected].nodeId, val)) {
                    status_text = "Status: Success (" + std::to_string(val).substr(0,4) + ")";
                } else {
                    status_text = "Status: Write Failed!";
                }
            } catch (...) {
                status_text = "Status: Invalid Number!";
            }
        }
    });

    auto layout = Container::Vertical({
        menu,
        input_field,
        btn,
    });

    auto renderer = Renderer(layout, [&] {
        client.updateValues();
        auto tags = client.getTags();
        Elements charts;

        for (const auto& tag : tags) {
            auto& history = histories[tag.name];
            history.push_back(tag.value);
            if (history.size() > 100) history.erase(history.begin());

            charts.push_back(vbox({
                text(tag.name + ": " + std::to_string(tag.value).substr(0, 6)) | bold | color(Color::Yellow),
                graph([&history](int w, int h) {
                    std::vector<int> r(w, 0);
                    if (history.empty() || h == 0) return r;
                    
                    double min_v = history[0], max_v = history[0];
                    for(auto v : history) {
                        if (v < min_v) min_v = v;
                        if (v > max_v) max_v = v;
                    }
                    if (max_v == min_v) { max_v += 1.0; min_v -= 1.0; }

                    for (int i = 0; i < w && i < history.size(); ++i) {
                        double val = history[history.size() - 1 - i];
                        r[w - 1 - i] = (int)((val - min_v) * h / (max_v - min_v));
                    }
                    return r;
                }) | flex | color(Color::GreenLight) | border
            }) | flex);
        }

        return vbox({
            text(" OPC UA MONITOR ") | center | bold | border | color(Color::Cyan),
            hbox({
                vbox({ text(" SELECT TAG: ") | bold, menu->Render() | border }) | size(WIDTH, EQUAL, 20),
                vbox({
                    text(" CONTROL: ") | bold,
                    hbox(text(" New Value: "), input_field->Render() | border) | size(HEIGHT, EQUAL, 3),
                    btn->Render() | center | border,
                    text(status_text) | center | color(Color::GrayDark)
                }) | flex
            }) | size(HEIGHT, EQUAL, 10),
            separator(),
            hbox(std::move(charts)) | flex
        }) | border;
    });
    std::thread([&] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            screen.PostEvent(Event::Custom); 
        }
    }).detach();

    screen.Loop(renderer);

    return 0;
}