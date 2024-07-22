#include "CSV_Helper.h"
#include <fstream>
#include <sstream>
#include <iomanip>

#define CSV_SEPARATOR ","

const char* DATE_FORMAT_COMBINED = "%Y-%m-%d%t%H:%M:%S";
const char* DATE_FORMAT_DATE = "%d.%m.%Y";
const char* DATE_FORMAT_TIME = "%H:%M:%S";

// That's hacky...
void ANSII_2_UTF8(std::string& str) {
    for(int i = 0; i < str.size(); i++) {
        const char c = str[i];
        if(c < 0) {
            str.insert(str.begin() + i++, '\xc2');
        }
    }
}

bool ParseFromFile(const char *filename, std::vector<const char*> &labels, std::vector<std::vector<CSV_DATA_NUMERIC_FORMAT>> &data, std::vector<std::string>& sources, bool relative_time, unsigned long long* time_start) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    // EPOCH timestamp just wont fit inside float, so subtract 1.6M from it, to make it a tiny bit better
    long long start_time = sizeof(CSV_DATA_NUMERIC_FORMAT) <= 4 && !relative_time ? 1'600'000'000 : 0;

    bool is_time_separate = false;
    bool parsing_sources = false;
    size_t line_idx = 0; // row
    std::string line;
    std::vector<std::string> buffer;

    while (std::getline(file, line)) {
        char *line_cstr = new char[line.size() + 1];
        std::strcpy(line_cstr, line.c_str());
        char *token = std::strtok(line_cstr, CSV_SEPARATOR);
        size_t col_idx = 0; // column

        // we love using deprecated functions here
        for(; (token != nullptr); token = std::strtok(nullptr, CSV_SEPARATOR), col_idx++) {
            std::string entry(token);
            if (line_idx == 0)
                ANSII_2_UTF8(entry);


            if (line_idx > 1 && ((col_idx == 0 && (entry == "Date/Time" || entry == "Date")) || (is_time_separate && col_idx == 1 && entry == "Time"))) {
                parsing_sources = true;
                break;
            } else if (line_idx == 0) {  // Parse labels
                if (col_idx == 0 && entry == "Date")
                    is_time_separate = true;
                if (entry.empty()) return false;
                char* _temp = new char[entry.size() + 1];
                strcpy_s(_temp, entry.size() + 1, entry.c_str());
                labels.push_back(_temp);
                continue;
            }
            else if (line_idx == 1)
                data.resize(labels.size());

            if (parsing_sources) { // Last row (data sources)
                sources.push_back(entry);
                continue;
            }

            CSV_DATA_NUMERIC_FORMAT value;
            try {
                // Parse Date - Time
                if (is_time_separate && col_idx < 2) {
                    static std::tm _date_time_part{};
                    if (col_idx == 0) {
                        std::istringstream(entry) >> std::get_time(&_date_time_part, DATE_FORMAT_DATE);
                    } else {
                        std::tm _time{};
                        if (std::istringstream(entry) >> std::get_time(&_time, DATE_FORMAT_TIME)) {
                            std::memcpy((char*)&_time + (sizeof(int) * 3), (char*)&_date_time_part + (sizeof(int) * 3), sizeof(std::tm) - (sizeof(int) * 3));
                            long long parsed_time = std::mktime(&_time);
                            if (relative_time && !start_time) {
                                start_time = parsed_time;
                            }
                            data[0].push_back(parsed_time - start_time);
                        }
                    }
                } else if (col_idx == 0) { // Parse Date/Time
                    std::tm _time{};
                    // Deal with ISO-8610 encoded date/time
                    // (its uses "T" instead of a white space to separate time from date)
                    if(auto it = entry.find('T'); it != std::string::npos)
                        entry[it] = ' ';

                    if (std::istringstream(entry) >> std::get_time(&_time, DATE_FORMAT_COMBINED)) {
                        long long parsed_time = std::mktime(&_time);
                        if (relative_time && !start_time) {
                            start_time = parsed_time;
                        }
                        data[col_idx].push_back(parsed_time - start_time);
                    }
                } else {
                    if (tolower(entry[0]) == 'n')
                        data[col_idx].push_back(0);
                    else if (tolower(entry[0]) == 'y')
                        data[col_idx].push_back(1);
                    else {
                        value = std::stof(entry);
                        data[col_idx].push_back(value);
                    }
                }
            } catch (std::exception const &ex) {
                printf("Error in line %zu with text: %s\n", line_idx, entry.c_str());
                // We can either take it on
                data[col_idx].push_back(-1); // Pretty awkward situation, let's call it "-1"
                // Or we can
                return false;
            }
        }

        delete[] line_cstr;
        line_idx++;
    }

    if (time_start) *time_start = start_time;

    return true;
}
