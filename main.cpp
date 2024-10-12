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
float avgTimeStep = 0.0f;

int data_start_idx = 1; // Used to skip first (Date/Time) or first two (Date, Time) columns

#define PLOT_SAMPLES_MARGIN 20 // Used only with AGGRESSIVE_GRAPH_CULLING defined
#define HASH_RANDOMNESS 13

//#define AGGRESSIVE_GRAPH_CULLING // It is even slower!

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
        cachedPlots[i] = CachedPlot(i, ImColor::HSV((float)HashString<unsigned short>(labels[i]) / (1 << (sizeof(short) * 8)), 0.65f, 1.f, 1.f));
    }

    avgTimeStep = (data[0].back() - data[0].front()) / data[0].size();
    printf("avg step = %.2f\n", avgTimeStep);
}

#ifdef _DEBUG
#include <chrono>
#endif
bool TryParseFile(const char* filename) {
#ifdef _DEBUG
    using namespace std::chrono;
    auto start = high_resolution_clock::now();
#endif

    if(!ParseFromFile(filename, labels, data, sources, use_relative_time, &imported_time_start))
        return false;

    if( labels.size() < 2 || data.size() < 2 || labels.size() != data.size() || data[0].empty()) {
        return false;
    }

#ifdef _DEBUG
    auto stop = high_resolution_clock::now();
    printf("Parsing took %lldms\n", duration_cast<microseconds>(stop - start).count() / 1000);
#endif

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

void OpenHWI_File() {
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

        if(!TryParseFile(filename))
            return;
    }
}

void HandleDropFile(WPARAM wparam) {
    // DragQueryFile() takes a LPWSTR for the name, so we need a TCHAR string
    TCHAR szName[MAX_PATH];

    // Here we cast the wParam as a HDROP handle to pass into the next functions
    HDROP hDrop = (HDROP)wparam;

    // Retrieve the first (0 index) file dropped
    DragQueryFile(hDrop, 0, szName, MAX_PATH);

    memset(filter_text, 0, sizeof(filter_text));
    labels.clear(); data.clear(); sources.clear(); cachedPlots.clear(); og_date.clear(); data_start_idx = 1;

    TryParseFile(szName);

    DragFinish(hDrop);
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
        static float stride_multi = 4;
        static std::vector<const char*> YA_label = {"[drop here]", "[drop here]"};

        ImGui::SetNextWindowSizeConstraints({220, 0}, {FLT_MAX, FLT_MAX});
        ImGui::BeginChild("Left Menu", ImVec2(220,-1), ImGuiChildFlags_ResizeX);

        if(ImGui::Button("Open File")) {
            OpenHWI_File();
        }
        ImGui::SameLine();
        if(ImGui::Checkbox("Use relative time", &use_relative_time) && !data.empty()) {
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

        ImGui::DragFloat("Plot Res.", &stride_multi, 0.1f, 0.1, 10, "%.2f");
        ImGui::SetItemTooltip("Might cause slight visual glitches when used with low values");

        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputTextWithHint("##Plot_Search_Box", "Search", filter_text, 256)) {
            std::string str = std::string(filter_text);
            std::transform(str.begin(), str.end(), str.begin(), std::tolower);
            for(int i = 0; i < cachedPlots.size(); i++) {
                cachedPlots[i].show = strstr(labels_lowercase[i], str.c_str());
            }
        }

        ImGui::BeginChild("Items Left Menu",ImVec2(-1,-1), ImGuiChildFlags_Border);

        // Push the color for the active plot elements
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        for (int k = data_start_idx; k < cachedPlots.size(); ++k) {
            if(!cachedPlots[k].show) continue;
            ImGui::PushID(k);
            ImPlot::ItemIcon((ImVec4)cachedPlots[k].color, 20);
            if(ImGui::BeginPopupContextItem()) {
                ImGui::ColorPicker4("Select Color", (float*)&cachedPlots[k].color);
                //ImGui::ColorEdit4("Select Color", (float*)&cachedPlots[k].color);
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Selectable(labels[k], cachedPlots[k].plot_idx != -1);
            if(k < sources.size())
                ImGui::SetItemTooltip("%s", sources[k]);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PLOT_DND", &k, sizeof(int));
                ImPlot::ItemIcon((ImVec4)cachedPlots[k].color); ImGui::SameLine();
                ImGui::TextUnformatted(labels[k]);
                ImGui::EndDragDropSource();
            }
            if(ImGui::IsItemHovered()) {
                // Double Left-Click to add the item to the first Y axis
                if(ImGui::IsMouseDoubleClicked(0)) {
                    cachedPlots[k].plot_idx = 0;
                    cachedPlots[k].Yax = ImAxis_Y1;
                    YA_label[0] = units[k].c_str();
                }
                // Double Right-Click to add the item to the second Y axis
                else if (ImGui::IsMouseDoubleClicked(1)) {
                    cachedPlots[k].plot_idx = 0;
                    cachedPlots[k].Yax = ImAxis_Y2;
                    YA_label[1] = units[k].c_str();
                }
                // Single Right-Click to remove the item
                else if(cachedPlots[k].plot_idx >= 0 && ImGui::IsItemClicked(1)) {
                    cachedPlots[k].plot_idx = -1;
                }
            }
        }
        ImGui::PopStyleColor();
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

        static ImVec2 last_plot_size {avail};
        static ImPlotRect last_plot_limits (0, 500, 0, 1);

        if(ImPlot::BeginPlot(open_file_name, {-1, -1})) {
            ImPlot::SetupAxis(ImAxis_X1, "time");
            if(!use_relative_time)
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            ImPlot::SetupAxis(ImAxis_Y1, YA_label[0]);
            ImPlot::SetupAxis(ImAxis_Y2, YA_label[1], ImPlotAxisFlags_Opposite);
//            ImGui::Text("Limits: [%.2f, %.2f]", last_plot_limits.X.Min, last_plot_limits.X.Max);
//            ImGui::Text("Size: [%.2f, %.2f]", last_plot_size.x, last_plot_size.y);

            if(!data.empty()) {

                float plot_x_range = last_plot_limits.X.Max - last_plot_limits.X.Min;
                static int stride = 0;
                stride = max(1, ((int) plot_x_range / last_plot_size.x) / stride_multi + 1);

                //data[0].back() / data[0].size();

#ifdef AGGRESSIVE_GRAPH_CULLING
                float visible_samples = plot_x_range / avgTimeStep;
                float scale_factor = data[0].size() / plot_x_range * avgTimeStep;
                float min_plot_size = min(
                        min(last_plot_size.x * stride_multi + 2, visible_samples + PLOT_SAMPLES_MARGIN),
                        data[0].size());
                static float samplesJump = 0; // Stride
                samplesJump = data[0].size() / min_plot_size;
                static int offset = 0;
                offset = max(1, (last_plot_limits.X.Min - data[0].front()) / avgTimeStep);
                samplesJump = max(1, samplesJump / scale_factor);
                samplesJump = max(1, plot_x_range / min_plot_size / avgTimeStep);
                float items_overflow_r = max(0, last_plot_limits.X.Max - data[0].back()) / plot_x_range;
                float items_overflow_l = min(0, last_plot_limits.X.Min - data[0].front()) / -plot_x_range;
                int items_outside_of_bounds = (items_overflow_r + items_overflow_l) * min_plot_size;
                //offset = (max(0, last_plot_limits.X.Min - data[0].front()) / plot_x_range) * min_plot_size * samplesJump;

//                ImGui::Text(
//                        "scale factor = %.3f, min_plot_size.x = %f, display scale = %.2f, offset = %f, right 0verflow: %.2f, left 0verflow: %.2f, OOB: %i, OOB frac %.2f, visible samples: %.2f\n",
//                        scale_factor, min_plot_size, samplesJump, offset * avgTimeStep, items_overflow_r,
//                        items_overflow_l, items_outside_of_bounds, items_overflow_r + items_overflow_l,
//                        visible_samples);
#endif

                static int i = 1;
                for (i = 1; i < cachedPlots.size(); i++) {
                    if (cachedPlots[i].plot_idx != 0) continue;

                    int plot_x_items = data[i].size() / stride;

                    ImPlot::SetAxis(cachedPlots[i].Yax);
                    ImPlot::SetNextLineStyle(cachedPlots[i].color);
                    ImPlot::SetNextFillStyle(cachedPlots[i].color, 0.25);
                    if (shaded_graphs)
#ifndef AGGRESSIVE_GRAPH_CULLING
                        ImPlot::PlotShaded(labels[i], data[0].data(), data[i].data(), plot_x_items, 0, 0, 0, sizeof(CSV_DATA_NUMERIC_FORMAT) * stride);
#else
                        ImPlot::PlotShadedG(labels[i], [](int k, void *data) {
                            auto *d = (std::vector<CSV_DATA_NUMERIC_FORMAT> *) data;
                            int idx = k * samplesJump;//min(d->size() - 1, k * samplesJump + offset);
                            return ImPlotPoint(d[0][idx], d[i][(idx)]);
                        }, data.data(), [](int k, void *data) {
                            auto *d = (std::vector<CSV_DATA_NUMERIC_FORMAT> *) data;
                            int idx = k * samplesJump;//min(d->size() - 1, k * samplesJump + offset);
                            return ImPlotPoint(d[0][idx], 0);
                        }, data.data(), data[i].size() / samplesJump);
#endif

                    if (scatter_plot)
#ifndef AGGRESSIVE_GRAPH_CULLING
                        ImPlot::PlotScatter(labels[i], data[0].data(), data[i].data(), plot_x_items, 0, 0, sizeof(CSV_DATA_NUMERIC_FORMAT) * stride);
#else
                        ImPlot::PlotScatterG(labels[i], [](int k, void *data) {
                            auto *d = (std::vector<CSV_DATA_NUMERIC_FORMAT> *) data;
                            int idx = min(d->size() - 1, k * samplesJump + offset);
                            return ImPlotPoint(d[0][idx], d[i][(idx)]);
                        }, data[i].data(), min_plot_size - items_outside_of_bounds);
#endif
                    else
#ifndef AGGRESSIVE_GRAPH_CULLING
                        ImPlot::PlotLine(labels[i], data[0].data(), data[i].data(), plot_x_items, 0, 0, sizeof(CSV_DATA_NUMERIC_FORMAT) * stride);
#else
                        ImPlot::PlotLineG(labels[i], [](int k, void *data) {
                            auto *d = (std::vector<CSV_DATA_NUMERIC_FORMAT> *) data;
                            int idx = min(d->size() - 1, k * samplesJump + offset);
                            return ImPlotPoint(d[0][idx], d[i][(idx)]);
                        }, data.data(), min_plot_size - items_outside_of_bounds);
#endif

                    // allow legend item labels to be DND sources
                    if (ImPlot::BeginDragDropSourceItem(labels[i])) {
                        ImGui::SetDragDropPayload("PLOT_DND", &i, sizeof(int));
                        ImPlot::ItemIcon((ImVec4) cachedPlots[i].color);
                        ImGui::SameLine();
                        ImGui::TextUnformatted(labels[i]);
                        ImPlot::EndDragDropSource();
                    }
                }

            }

            last_plot_size = ImPlot::GetPlotSize();
            last_plot_limits = ImPlot::GetPlotLimits();

            // Y1, Y2 axis Drag-N-Drop
            for (int y = ImAxis_Y1; y <= ImAxis_Y2; ++y) {
                if (ImPlot::BeginDragDropTargetAxis(y)) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                        int i = *(int*)payload->Data; cachedPlots[i].plot_idx = 0; cachedPlots[i].Yax = y;
                        YA_label[y - ImAxis_Y1] = units[i].c_str();
                    }
                    ImPlot::EndDragDropTarget();
                }
            }

            // Plot Drag-N-Drop
            if (ImPlot::BeginDragDropTargetPlot()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLOT_DND")) {
                    int i = *(int*)payload->Data; cachedPlots[i].plot_idx = 0;
                }
                ImPlot::EndDragDropTarget();
            }

            // Legend Drag-N-Drop
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

#ifndef _DEBUG
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine,
                     _In_ int nShowCmd) {
#else
int main() {
#endif
    hwnd = GUI::Setup(DrawGui);

    ImGui::GetIO().IniFilename = nullptr;

    GUI::OnFileDrop = HandleDropFile;

    while(true) {
        if(GUI::DrawFrame())
            break;
    }

    GUI::Destroy();

    return 0;
}
