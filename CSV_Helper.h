#include <vector>
#include <string>

#define CSV_DATA_NUMERIC_FORMAT float

// converts data to seconds since 1970, and "YES" / "NO" to 1 / 0 respectively. "sources" is just the last row
bool ParseFromFile(const char* filename, std::vector<const char*>& labels, std::vector<std::vector<CSV_DATA_NUMERIC_FORMAT>> &data, std::vector<std::string>& sources, bool relative_time = true, unsigned long long* time_start = nullptr);