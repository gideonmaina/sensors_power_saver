#include <ArduinoJson.h>
template <typename T>
void generateCSV_payload(char *res, size_t res_size, const char *timestamp, const char *value_type, T value, const char *unit, const char *sensor_type)
{
    // Use type traits to check type at compile time
    if constexpr (std::is_integral<T>::value)
    {
        snprintf(res, res_size, "%s,%s,%d,%s,%s", timestamp, value_type, static_cast<int>(value), unit, sensor_type);
    }
    else if constexpr (std::is_floating_point<T>::value)
    {
        snprintf(res, res_size, "%s,%s,%.2f,%s,%s", timestamp, value_type, static_cast<double>(value), unit, sensor_type);
    }
    else if constexpr (std::is_same<T, const char *>::value || std::is_same<T, char *>::value)
    {
        snprintf(res, res_size, "%s,%s,%s,%s,%s", timestamp, value_type, value, unit, sensor_type);
    }
    else
    {
        Serial.println("Unsupported type for CSV payload generation");
    }
}

/**
    @brief Add value to JSON array
    @param arr : JSON array to add the value to
    @param key : key for the value
    @param value : value to add
    @return : void
**/
template <typename T>
void add_value2JSON_array(JsonArray arr, const char *key, T &value)
{
    JsonDocument doc;
    doc["value_type"] = key;
    doc["value"] = value;
    arr.add(doc);
}

/**
    @brief Validate JSON data
    @param input : JSON data to validate
    @return : true if valid, false otherwise
    @note : //! This function is not full proof. Raw strings that don't look like incomplete or empty JSON are validated.
**/
bool validateJson(const char *input)
{
    JsonDocument doc, filter;
    return deserializeJson(doc, input, DeserializationOption::Filter(filter)) == DeserializationError::Ok;
}