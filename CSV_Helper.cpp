#include "CSV_Helper.h"
#include <fstream>
#include <sstream>
#include <iomanip>

// I think there are multiple formats /;
// I'm using this one "2023-03-18 18:47:50.606Z"
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

    // EPOCH timestamp just wont fit inside float, so subtract 1.6M from it, to make it a tiny bit better
    long long start_time = sizeof(CSV_DATA_NUMERIC_FORMAT) <= 4 && !relative_time ? 1'600'000'000 : 0;

    if(!file.good())
        return false;

    bool is_time_separate = false;
    bool parsing_sources = false;
    size_t line_idx = 0; // row
    std::string line;
    while(std::getline(file, line)) {
        std::istringstream ss(line);

        size_t col_idx = 0; // column
        std::string entry;
        while(std::getline(ss, entry, ',')) {
            if((is_time_separate && col_idx == 2) || col_idx == 0)
                ANSII_2_UTF8(entry);
            if(line_idx > 1 && ((col_idx == 0 && (entry == "Date/Time" || entry == "Date")) || (is_time_separate && col_idx == 1 && entry == "Time"))) { // just a copy of the first row (labels)
                parsing_sources = true;
                break;
            }
            else if (line_idx == 0) // Parse labels
            {
                if(col_idx == 0 && entry == "Date")
                    is_time_separate = true;
                if(entry.empty()) return false;
                char* _temp = new char[entry.size() + 1];
                strcpy_s(_temp, entry.size() + 1, entry.c_str());
                labels.push_back(_temp);
                continue;
            }
            else
                data.resize(labels.size());
            if(parsing_sources) { // Last row (data sources)
                sources.push_back(entry);
                continue;
            }

            // Normal data
            CSV_DATA_NUMERIC_FORMAT value;
            try {
                if(is_time_separate && col_idx < 2) {
                    static std::tm _date_time_part{};
                    if(col_idx == 0) {
                        std::stringstream(entry) >> std::get_time(&_date_time_part, DATE_FORMAT_DATE);
                    }
                    else if(col_idx == 1) {
                        std::tm _time{};
                        if(std::stringstream(entry) >> std::get_time(&_time, DATE_FORMAT_TIME)) {
                            memcpy((char*)&_time + (sizeof(int) * 3), (char*)&_date_time_part + (sizeof(int) * 3), sizeof(std::tm) - (sizeof(int) * 3));
                            //printf("Time: %lld\n", std::mktime(&_time));
                            long long parsed_time = std::mktime(&_time);
                            if(relative_time && !start_time) {
                                start_time = parsed_time;
                            }
                            data[0].push_back(parsed_time - start_time);
                        }
                    }
                }
                else if(col_idx == 0) {
                    std::tm _time{};
                    if(std::stringstream(entry) >> std::get_time(&_time, DATE_FORMAT_COMBINED)) {
                        //printf("Time: %lld\n", std::mktime(&_time));
                        long long parsed_time = std::mktime(&_time);
                        if(relative_time && !start_time) {
                            start_time = parsed_time;
                        }
                        data[col_idx].push_back(parsed_time - start_time);
                    }
                }
                else {
                    value = std::stof(entry);
                    data[col_idx].push_back(value);
                }
            }
            catch (std::exception const &ex) {
                // idk, this is pretty bad
                data[col_idx].push_back(-1); // call it... um... -1
            }

            //printf("entry = %s\n", entry.c_str());
            col_idx++;
        }

        line_idx++;
    }

    if(time_start) *time_start = start_time;

    return true;
}
