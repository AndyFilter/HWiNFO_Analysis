#include <cstdio>
#include <algorithm>
#include "GUI/gui.h"
#include "External/ImGui/imgui.h"
#include "External/ImGui/implot.h"
#include "CSV_Helper.h"

HWND hwnd = nullptr;

std::vector<const char*> labels;
std::vector<const char*> labels_lowercase;
std::vector<const char*> sources;
std::vector<std::string> units;
std::vector<std::vector<CSV_DATA_NUMERIC_FORMAT>> data;
std::vector<CSV_DATA_NUMERIC_FORMAT> og_date;
char filter_text[256] = {0};
char* open_file_name = new char[MAX_PATH] {'P', 'l', 'o', 't', 0};
bool use_relative_time = true;
unsigned long long imported_time_start = 0;

int data_start_idx = 1; // Used to skip first (Date/Time) or first two (Date, Time) columns

#define HASH_RANDOMNESS 13

//std::vector<int> selected_variables{0};

// A pretty bad way of hashing, but I just wanted to get some random values, so it works
template<typename Ty>
Ty HashString(const char* szText) {
    size_t str_len = strlen(szText);
    if(str_len == 0) return 0;
    union Data {
        Ty val = 0;
        unsigned char data[sizeof(Ty)];
    } hash;
    for(int i = 0; i < max((sizeof(Ty)), str_len); i++) {
        hash.data[i % sizeof(Ty)] ^= (unsigned char)((szText[i % str_len] ^ i) * HASH_RANDOMNESS);
    }
    if constexpr (std::is_floating_point<Ty>::value)
        if(isnan(hash.val)) *hash.data ^= 0x7ff0000000000001;
    return hash.val;
}

// For Drag-N-Drop
struct CachedPlot {
    size_t idx = 0;
    ImColor color = 0;
    int plot_idx = -1;
    int Yax = 0;
    bool show = true;

    CachedPlot(size_t idx, const ImColor &color) : idx(idx), color(color) {};
    CachedPlot() = default;
};

// Generate colors (and other stuff in the future) for plots icons / preview
std::vector<CachedPlot> cachedPlots;
void PreCacheGraphs() {
    cachedPlots.resize(labels.size());

    for (int i = 0; i < cachedPlots.size(); i++) {
        cachedPlots[i] = CachedPlot(i, ImColor::HSV((float)HashString<unsigned short>(labels[i]) / (1 << (sizeof(short) * 8)), 0.5f, 1.f, 1.f));
    }
}

#include <chrono>
void OpenHWI_File() {
    using namespace std::chrono;
    char filename[MAX_PATH]{ "" };

    const char szExt[] = "CSV\0*.csv\0\0";

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = ofn.lpstrDefExt = szExt;
    ofn.Flags = OFN_DONTADDTORECENT;

    if (GetOpenFileName(&ofn))
    {
        memset(filter_text, 0, sizeof(filter_text));
        labels.clear(); data.clear(); sources.clear(); cachedPlots.clear(); og_date.clear(); data_start_idx = 1;

        auto start = high_resolution_clock::now();
        if(!ParseFromFile(filename, labels, data, sources, true, &imported_time_start))
            return;
        auto stop = high_resolution_clock::now();
        printf("Parsing took %lldms\n", duration_cast<microseconds>(stop - start).count() / 1000);

        og_date = data[0];

        auto full_file_name = std::string_view (filename);
        auto file_name = full_file_name.substr(full_file_name.find_last_of("/\\") + 1);
        file_name = file_name.substr(0, file_name.find_last_of('.'));
        file_name.copy(open_file_name, file_name.size() + 1);
        open_file_name[file_name.size()] = 0;

        labels_lowercase.resize(labels.size());
        units.resize(labels.size());

        // parse all labels ignoring the case for faster search
        // and extract the units from the labels
        for(int i = 0; i < labels.size(); i++) {
            auto len = strlen(labels[i]);
            char* tmp = new char[len + 1];
            tmp[len] = 0;
            for(int j = 0; labels[i][j]; j++)
                tmp[j] = tolower(labels[i][j]);
            labels_lowercase[i] = tmp;
            auto open_bracket = strrchr(labels[i], '[');
            if(open_bracket) {
                units[i] = std::string(std::string_view(open_bracket, labels[i] - open_bracket + len - 1));
            }
        }
        PreCacheGraphs();

        if(std::string(labels[1]) == "Time") {
            data_start_idx = 2;
        }
    }
}

int DrawGui() {
    auto& io = ImGui::GetIO();
    auto avail = ImGui::GetContentRegionAvail();

    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    if(ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::PopStyleVar();
        static bool shaded_graphs = true;
        static bool scatter_plot = false;

        ImGui::SetNextWindowSizeConstraints({220, 0}, {FLT_MAX, FLT_MAX});
        ImGui::BeginChild("Left Menu",ImVec2(220,-1), ImGuiChildFlags_ResizeX);

        if(ImGui::Button("Open File")) {
            OpenHWI_File();
        }
        ImGui::SameLine();
        if(ImGui::Checkbox("Use relative time", &use_relative_time)) {
            for(int i = 0; i < data[0].size(); i++) {
                if(use_relative_time)
                    data[0][i] = og_date[i];
                else
                    data[0][i] += (CSV_DATA_NUMERIC_FORMAT)imported_time_start;
            }
        }
        ImGui::SetItemTooltip("Sets the first entry time to 0 and all later timestamps are relative to the first one");
        ImGui::Checkbox("Shaded graphs", &shaded_graphs);
        ImGui::SameLine();
        ImGui::Checkbox("Scatter plot", &scatter_plot);

        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputTextWithHint("##Plot_Search_Box", "Search", filter_text, 256)) {
            std::string str = std::string(filter_text);
            std::transform(str.begin(), str.end(), str.begin(), std::tolower);
            for(int i = 0; i < cachedPlots.size(); i++) {
                cachedPlots[i].show = strstr(labels_lowercase[i], str.c_str());
            }
        }

        ImGui::BeginChild("Items Left Menu",ImVec2(-1,-1), ImGuiChildFlags_Border);

        for (int k = data_start_idx; k < cachedPlots.size(); ++k) {
            if(!cachedPlots[k].show) continue;
            ImPlot::ItemIcon((ImVec4)cachedPlots[k].color, 20); ImGui::SameLine();
            ImGui::Selectable(labels[k], false, 0);
            if(k < sources.size())
                ImGui::SetItemTooltip("%s", sources[k]);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PLOT_DND", &k, sizeof(int));
                ImPlot::ItemIcon((ImVec4)cachedPlots[k].color); ImGui::SameLine();
                ImGui::TextUnformatted(labels[k]);
                ImGui::EndDragDropSource();
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                int i = *(int*)payload->Data; cachedPlots[i].plot_idx = -1;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::BeginGroup();


        static std::vector<const char*> YA_label = {"[drop here]", "[drop here]"};

        if(ImPlot::BeginPlot(open_file_name, {-1, -1})) {
            ImPlot::SetupAxis(ImAxis_X1, "time");
            if(!use_relative_time)
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            ImPlot::SetupAxis(ImAxis_Y1, YA_label[0]);
            ImPlot::SetupAxis(ImAxis_Y2, YA_label[1], ImPlotAxisFlags_Opposite);
            for(int i = 1; i < cachedPlots.size(); i++) {
                if(cachedPlots[i].plot_idx != 0) continue;

                ImPlot::SetAxis(cachedPlots[i].Yax);
                ImPlot::SetNextLineStyle(cachedPlots[i].color);
                ImPlot::SetNextFillStyle(cachedPlots[i].color, 0.25);
                if(shaded_graphs)
                    ImPlot::PlotShaded(labels[i], data[0].data(), data[i].data(), data[i].size());
                if(scatter_plot)
                    ImPlot::PlotScatter(labels[i], data[0].data(), data[i].data(), data[i].size());
                else
                    ImPlot::PlotLine(labels[i], data[0].data(), data[i].data(), data[i].size());

                // allow legend item labels to be DND sources
                if (ImPlot::BeginDragDropSourceItem(labels[i])) {
                    ImGui::SetDragDropPayload("PLOT_DND", &i, sizeof(int));
                    ImPlot::ItemIcon((ImVec4)cachedPlots[i].color); ImGui::SameLine();
                    ImGui::TextUnformatted(labels[i]);
                    ImPlot::EndDragDropSource();
                }
            }

            for (int y = ImAxis_Y1; y <= ImAxis_Y2; ++y) {
                if (ImPlot::BeginDragDropTargetAxis(y)) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                        int i = *(int*)payload->Data; cachedPlots[i].plot_idx = 0; cachedPlots[i].Yax = y;
                        YA_label[y - ImAxis_Y1] = units[i].c_str();
                    }
                    ImPlot::EndDragDropTarget();
                }
            }

            if (ImPlot::BeginDragDropTargetPlot()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                    int i = *(int*)payload->Data; cachedPlots[i].plot_idx = 0;
                }
                ImPlot::EndDragDropTarget();
            }

            if (ImPlot::BeginDragDropTargetLegend()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                    int i = *(int*)payload->Data; cachedPlots[i].plot_idx = 0;
                }
                ImPlot::EndDragDropTarget();
            }

            ImPlot::EndPlot();
        }

        ImGui::EndGroup();
    }
    else
        ImGui::PopStyleVar();
    ImGui::End();

    return 0;
}

int main() {
    hwnd = GUI::Setup(DrawGui);

    ImGui::GetIO().IniFilename = nullptr;

    while(true) {
        if(GUI::DrawFrame())
            break;
    }

    GUI::Destroy();

    return 0;
}
